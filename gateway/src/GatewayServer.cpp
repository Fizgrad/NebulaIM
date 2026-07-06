#include "GatewayServer.h"

#include "common/error/ErrorCode.h"
#include "common/gateway/GatewayPacketHelper.h"
#include "common/log/Logger.h"
#include "common/monitor/MetricsRegistry.h"
#include "common/net/EventLoop.h"
#include "common/net/TcpConnection.h"
#include "common/utils/TimeUtil.h"
#include "common/websocket/WebSocketHandshake.h"

namespace nebula {

namespace {
bool startsWithGet(const Buffer* buffer) {
    return buffer != nullptr && buffer->readableBytes() >= 4 && std::string(buffer->peek(), 4) == "GET ";
}
}

GatewayServer::GatewayServer(EventLoop* loop,
                             const InetAddress& listen_addr,
                             std::string gateway_id,
                             ConnectionManager* connection_manager,
                             GatewayOnlineManager* online_manager,
                             GatewayRouter* router,
                             int worker_threads,
                             int heartbeat_timeout_ms,
                             std::shared_ptr<TlsContext> tls_context)
    : loop_(loop), server_(loop, listen_addr, "GatewayTcpServer"), gateway_id_(std::move(gateway_id)),
      connection_manager_(connection_manager), online_manager_(online_manager), router_(router), heartbeat_timeout_ms_(heartbeat_timeout_ms) {
    server_.setThreadNum(worker_threads);
    server_.setTlsContext(std::move(tls_context));
    server_.setConnectionCallback([this](const TcpConnectionPtr& conn) { onConnection(conn); });
    server_.setMessageCallback([this](const TcpConnectionPtr& conn, Buffer* buffer) { onMessage(conn, buffer); });
    if (router_ != nullptr) {
        router_->setPacketSender([this](const TcpConnectionPtr& conn, const Packet& packet) { sendPacket(conn, packet); });
    }
}

void GatewayServer::start() {
    server_.start();
    loop_->runEvery(5000, [this]() { checkHeartbeatTimeout(); });
}

void GatewayServer::onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        std::string id = connection_manager_->addConnection(conn, conn->peerAddress().toIpPort());
        bindConnectionIdToConnection(conn, id);
        {
            std::lock_guard<std::mutex> lock(conn_id_mutex_);
            websocket_connections_[conn->name()] = false;
        }
        MetricsRegistry::instance().counter("nebula_gateway_connections_total", "Gateway accepted connections").inc();
        MetricsRegistry::instance().gauge("nebula_gateway_active_connections", "Gateway active connections").inc();
        LOG_INFO("gateway connection established id=" + id);
    } else {
        std::string id = getConnectionIdFromConnection(conn);
        auto ctx = connection_manager_->getContext(id);
        if (ctx.has_value() && ctx->authenticated) {
            online_manager_->setOfflineAsync(ctx->user_id, ctx->device_id, id);
        }
        connection_manager_->removeConnection(id);
        std::lock_guard<std::mutex> lock(conn_id_mutex_);
        conn_name_to_id_.erase(conn->name());
        websocket_connections_.erase(conn->name());
        MetricsRegistry::instance().gauge("nebula_gateway_active_connections", "Gateway active connections").dec();
        LOG_INFO("gateway connection closed id=" + id);
    }
}

void GatewayServer::onMessage(const TcpConnectionPtr& conn, Buffer* buffer) {
    std::string id = getConnectionIdFromConnection(conn);
    bool websocket = false;
    {
        std::lock_guard<std::mutex> lock(conn_id_mutex_);
        auto it = websocket_connections_.find(conn->name());
        websocket = it != websocket_connections_.end() && it->second;
    }
    if (!websocket && startsWithGet(buffer)) {
        if (!tryHandleWebSocketHandshake(conn, buffer)) return;
        {
            std::lock_guard<std::mutex> lock(conn_id_mutex_);
            websocket_connections_[conn->name()] = true;
        }
        if (connection_manager_ != nullptr) connection_manager_->markWebSocket(id, true);
        websocket = true;
    }
    if (!rate_limiter_.allowPacket(id)) {
        MetricsRegistry::instance().counter("nebula_gateway_rate_limited_total", "Gateway requests rejected by rate limiting").inc();
        Packet error = GatewayPacketHelper::makeErrorResponse(0, static_cast<int>(ErrorCode::RATE_LIMITED), "rate limited", "");
        sendPacket(conn, error);
        return;
    }
    if (websocket) {
        onWebSocketMessage(conn, buffer, id);
        return;
    }
    onTcpMessage(conn, buffer, id);
}

void GatewayServer::onTcpMessage(const TcpConnectionPtr& conn, Buffer* buffer, const std::string& id) {
    while (buffer->readableBytes() >= kPacketHeaderLength) {
        Packet packet;
        ProtocolError err = codec_.decode(buffer, &packet);
        if (err == ProtocolError::OK) router_->handlePacket(conn, id, packet);
        else if (err == ProtocolError::INCOMPLETE_PACKET) break;
        else {
            Packet error = GatewayPacketHelper::makeErrorResponse(0, static_cast<int>(ErrorCode::GATEWAY_INVALID_PACKET), protocolErrorToString(err), "");
            conn->send(codec_.encode(error));
            conn->forceClose();
            break;
        }
    }
}

void GatewayServer::onWebSocketMessage(const TcpConnectionPtr& conn, Buffer* buffer, const std::string& id) {
    while (buffer->readableBytes() > 0) {
        WebSocketFrame frame;
        WebSocketCodecStatus status = websocket_codec_.decodeFrame(buffer, &frame);
        if (status == WebSocketCodecStatus::INCOMPLETE) break;
        if (status == WebSocketCodecStatus::PROTOCOL_ERROR) {
            conn->send(websocket_codec_.encodeClose("protocol error"));
            conn->forceClose();
            return;
        }
        if (frame.opcode == WebSocketOpCode::PING) {
            conn->send(websocket_codec_.encodePong(frame.payload));
            continue;
        }
        if (frame.opcode == WebSocketOpCode::PONG) {
            continue;
        }
        if (frame.opcode == WebSocketOpCode::CLOSE) {
            conn->send(websocket_codec_.encodeClose());
            conn->forceClose();
            return;
        }
        if (frame.opcode != WebSocketOpCode::BINARY) {
            conn->send(websocket_codec_.encodeClose("binary frame required"));
            conn->forceClose();
            return;
        }

        Buffer packet_buffer;
        packet_buffer.append(frame.payload);
        while (packet_buffer.readableBytes() >= kPacketHeaderLength) {
            Packet packet;
            ProtocolError err = codec_.decode(&packet_buffer, &packet);
            if (err == ProtocolError::OK) router_->handlePacket(conn, id, packet);
            else if (err == ProtocolError::INCOMPLETE_PACKET) break;
            else {
                Packet error = GatewayPacketHelper::makeErrorResponse(0, static_cast<int>(ErrorCode::GATEWAY_INVALID_PACKET), protocolErrorToString(err), "");
                sendPacket(conn, error);
                conn->forceClose();
                return;
            }
        }
    }
}

bool GatewayServer::tryHandleWebSocketHandshake(const TcpConnectionPtr& conn, Buffer* buffer) {
    std::string request(buffer->peek(), buffer->readableBytes());
    auto pos = request.find("\r\n\r\n");
    if (pos == std::string::npos) {
        if (request.size() > 8192) {
            conn->send(WebSocketHandshake::badRequestResponse("header too large"));
            conn->forceClose();
        }
        return false;
    }
    std::string header = request.substr(0, pos + 4);
    WebSocketHandshakeResult result = WebSocketHandshake::buildServerResponse(header);
    conn->send(result.response);
    buffer->retrieve(pos + 4);
    if (!result.ok) {
        conn->forceClose();
        return false;
    }
    LOG_INFO("websocket handshake ok path=" + result.request.path);
    return true;
}

void GatewayServer::sendPacket(const TcpConnectionPtr& conn, const Packet& packet) {
    bool websocket = false;
    {
        std::lock_guard<std::mutex> lock(conn_id_mutex_);
        auto it = websocket_connections_.find(conn->name());
        websocket = it != websocket_connections_.end() && it->second;
    }
    std::string bytes = codec_.encode(packet);
    conn->send(websocket ? websocket_codec_.encodeBinary(bytes) : bytes);
}

void GatewayServer::checkHeartbeatTimeout() {
    auto ids = connection_manager_->getTimeoutConnections(TimeUtil::nowMs(), heartbeat_timeout_ms_);
    for (const auto& id : ids) {
        auto conn = connection_manager_->getConnection(id);
        if (conn) conn->forceClose();
    }
}

std::string GatewayServer::getConnectionIdFromConnection(const TcpConnectionPtr& conn) {
    std::lock_guard<std::mutex> lock(conn_id_mutex_);
    auto it = conn_name_to_id_.find(conn->name());
    return it == conn_name_to_id_.end() ? "" : it->second;
}

void GatewayServer::bindConnectionIdToConnection(const TcpConnectionPtr& conn, const std::string& connection_id) {
    std::lock_guard<std::mutex> lock(conn_id_mutex_);
    conn_name_to_id_[conn->name()] = connection_id;
}

}  // namespace nebula
