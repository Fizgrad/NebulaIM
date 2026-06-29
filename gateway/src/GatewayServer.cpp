#include "GatewayServer.h"

#include "common/error/ErrorCode.h"
#include "common/gateway/GatewayPacketHelper.h"
#include "common/log/Logger.h"
#include "common/net/EventLoop.h"
#include "common/net/TcpConnection.h"
#include "common/utils/TimeUtil.h"

namespace nebula {

GatewayServer::GatewayServer(EventLoop* loop,
                             const InetAddress& listen_addr,
                             std::string gateway_id,
                             ConnectionManager* connection_manager,
                             GatewayOnlineManager* online_manager,
                             GatewayRouter* router,
                             int worker_threads,
                             int heartbeat_timeout_ms)
    : loop_(loop), server_(loop, listen_addr, "GatewayTcpServer"), gateway_id_(std::move(gateway_id)),
      connection_manager_(connection_manager), online_manager_(online_manager), router_(router), heartbeat_timeout_ms_(heartbeat_timeout_ms) {
    server_.setThreadNum(worker_threads);
    server_.setConnectionCallback([this](const TcpConnectionPtr& conn) { onConnection(conn); });
    server_.setMessageCallback([this](const TcpConnectionPtr& conn, Buffer* buffer) { onMessage(conn, buffer); });
}

void GatewayServer::start() {
    server_.start();
    loop_->runEvery(5000, [this]() { checkHeartbeatTimeout(); });
}

void GatewayServer::onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        std::string id = connection_manager_->addConnection(conn, conn->peerAddress().toIpPort());
        bindConnectionIdToConnection(conn, id);
        LOG_INFO("gateway connection established id=" + id);
    } else {
        std::string id = getConnectionIdFromConnection(conn);
        auto ctx = connection_manager_->getContext(id);
        if (ctx.has_value() && ctx->authenticated) online_manager_->setOffline(ctx->user_id, id);
        connection_manager_->removeConnection(id);
        std::lock_guard<std::mutex> lock(conn_id_mutex_);
        conn_name_to_id_.erase(conn->name());
        LOG_INFO("gateway connection closed id=" + id);
    }
}

void GatewayServer::onMessage(const TcpConnectionPtr& conn, Buffer* buffer) {
    std::string id = getConnectionIdFromConnection(conn);
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
