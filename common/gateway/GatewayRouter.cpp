#include "common/gateway/GatewayRouter.h"

#include "common/error/ErrorCode.h"
#include "common/gateway/GatewayPacketHelper.h"
#include "common/log/Logger.h"
#include "common/net/TcpConnection.h"
#include "message.pb.h"
#include "user.pb.h"

#include <chrono>
#include <grpcpp/grpcpp.h>

namespace nebula {

namespace {
void setDeadline(grpc::ClientContext* ctx) {
    ctx->set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(2));
}
}

GatewayRouter::GatewayRouter(ConnectionManager* connection_manager, GatewayOnlineManager* online_manager, GatewayBackendClients* backend_clients, PacketCodec* codec)
    : connection_manager_(connection_manager), online_manager_(online_manager), backend_clients_(backend_clients), codec_(codec) {}

void GatewayRouter::handlePacket(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet) {
    if (connection_manager_) connection_manager_->updateActiveTime(connection_id);
    switch (packet.type) {
        case MessageType::LOGIN_REQ: handleLogin(conn, connection_id, packet); break;
        case MessageType::HEARTBEAT_REQ: handleHeartbeat(conn, connection_id, packet); break;
        case MessageType::SEND_SINGLE_MSG_REQ: handleSendSingleMessage(conn, connection_id, packet); break;
        case MessageType::SEND_GROUP_MSG_REQ: handleSendGroupMessage(conn, connection_id, packet); break;
        case MessageType::ACK_REQ: handleAck(conn, connection_id, packet); break;
        case MessageType::PULL_OFFLINE_MSG_REQ: handlePullOfflineMessages(conn, connection_id, packet); break;
        default: sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::GATEWAY_INVALID_PACKET), "unsupported packet", ""); break;
    }
}

void GatewayRouter::sendPacket(const TcpConnectionPtr& conn, const Packet& packet) {
    if (conn && codec_) conn->send(codec_->encode(packet));
}

void GatewayRouter::sendError(const TcpConnectionPtr& conn, uint32_t sequence_id, int code, const std::string& message, const std::string& request_id) {
    sendPacket(conn, GatewayPacketHelper::makeErrorResponse(sequence_id, code, message, request_id));
}

bool GatewayRouter::requireAuth(const TcpConnectionPtr& conn, const std::string& connection_id, uint32_t sequence_id, std::optional<ConnectionContext>* context) {
    auto ctx = connection_manager_->getContext(connection_id);
    if (!ctx.has_value() || !ctx->authenticated) {
        sendError(conn, sequence_id, static_cast<int>(ErrorCode::GATEWAY_NOT_AUTHENTICATED), "not authenticated", "");
        return false;
    }
    *context = ctx;
    return true;
}

void GatewayRouter::handleLogin(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet) {
    proto::LoginRequest req;
    if (!GatewayPacketHelper::parseBody(packet, &req)) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::GATEWAY_INVALID_PACKET), "bad login body", ""); return; }
    proto::LoginResponse resp;
    grpc::ClientContext ctx;
    setDeadline(&ctx);
    grpc::Status status = backend_clients_->userService()->Login(&ctx, req, &resp);
    if (status.ok() && resp.response().code() == 0) {
        connection_manager_->bindUser(connection_id, resp.user_id(), resp.token());
        if (!online_manager_->setOnline(resp.user_id(), connection_id)) {
            resp.mutable_response()->set_code(static_cast<int>(ErrorCode::GATEWAY_ONLINE_STATE_FAILED));
            resp.mutable_response()->set_message("online state failed");
        }
    } else if (!status.ok()) {
        resp.mutable_response()->set_code(static_cast<int>(ErrorCode::GATEWAY_BACKEND_RPC_FAILED));
        resp.mutable_response()->set_message(status.error_message());
        resp.mutable_response()->set_request_id(req.request_id());
    }
    sendPacket(conn, GatewayPacketHelper::makeResponse(MessageType::LOGIN_RESP, packet.sequence_id, resp));
}

void GatewayRouter::handleHeartbeat(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet) {
    auto ctx = connection_manager_->getContext(connection_id);
    if (ctx.has_value() && ctx->authenticated) online_manager_->refreshOnline(ctx->user_id, connection_id);
    proto::CommonResponse resp;
    resp.set_code(0);
    resp.set_message("OK");
    sendPacket(conn, GatewayPacketHelper::makeResponse(MessageType::HEARTBEAT_RESP, packet.sequence_id, resp));
}

void GatewayRouter::handleSendSingleMessage(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet) {
    std::optional<ConnectionContext> auth;
    if (!requireAuth(conn, connection_id, packet.sequence_id, &auth)) return;
    proto::SendSingleMessageRequest req;
    if (!GatewayPacketHelper::parseBody(packet, &req)) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::GATEWAY_INVALID_PACKET), "bad single body", ""); return; }
    if (req.from_user_id() == 0) req.set_from_user_id(auth->user_id);
    if (req.from_user_id() != auth->user_id) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::GATEWAY_PERMISSION_DENIED), "permission denied", req.request_id()); return; }
    proto::SendSingleMessageResponse resp;
    grpc::ClientContext ctx;
    setDeadline(&ctx);
    grpc::Status status = backend_clients_->messageService()->SendSingleMessage(&ctx, req, &resp);
    if (!status.ok()) { resp.mutable_response()->set_code(static_cast<int>(ErrorCode::GATEWAY_BACKEND_RPC_FAILED)); resp.mutable_response()->set_message(status.error_message()); resp.mutable_response()->set_request_id(req.request_id()); }
    sendPacket(conn, GatewayPacketHelper::makeResponse(MessageType::SEND_SINGLE_MSG_RESP, packet.sequence_id, resp));
}

void GatewayRouter::handleSendGroupMessage(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet) {
    std::optional<ConnectionContext> auth;
    if (!requireAuth(conn, connection_id, packet.sequence_id, &auth)) return;
    proto::SendGroupMessageRequest req;
    if (!GatewayPacketHelper::parseBody(packet, &req)) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::GATEWAY_INVALID_PACKET), "bad group body", ""); return; }
    if (req.from_user_id() == 0) req.set_from_user_id(auth->user_id);
    if (req.from_user_id() != auth->user_id) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::GATEWAY_PERMISSION_DENIED), "permission denied", req.request_id()); return; }
    proto::SendGroupMessageResponse resp;
    grpc::ClientContext ctx;
    setDeadline(&ctx);
    grpc::Status status = backend_clients_->messageService()->SendGroupMessage(&ctx, req, &resp);
    if (!status.ok()) { resp.mutable_response()->set_code(static_cast<int>(ErrorCode::GATEWAY_BACKEND_RPC_FAILED)); resp.mutable_response()->set_message(status.error_message()); resp.mutable_response()->set_request_id(req.request_id()); }
    sendPacket(conn, GatewayPacketHelper::makeResponse(MessageType::SEND_GROUP_MSG_RESP, packet.sequence_id, resp));
}

void GatewayRouter::handleAck(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet) {
    std::optional<ConnectionContext> auth;
    if (!requireAuth(conn, connection_id, packet.sequence_id, &auth)) return;
    proto::AckMessageRequest req;
    if (!GatewayPacketHelper::parseBody(packet, &req)) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::GATEWAY_INVALID_PACKET), "bad ack body", ""); return; }
    if (req.user_id() == 0) req.set_user_id(auth->user_id);
    if (req.user_id() != auth->user_id) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::GATEWAY_PERMISSION_DENIED), "permission denied", req.request_id()); return; }
    proto::AckMessageResponse resp;
    grpc::ClientContext ctx;
    setDeadline(&ctx);
    grpc::Status status = backend_clients_->messageService()->AckMessage(&ctx, req, &resp);
    if (!status.ok()) { resp.mutable_response()->set_code(static_cast<int>(ErrorCode::GATEWAY_BACKEND_RPC_FAILED)); resp.mutable_response()->set_message(status.error_message()); resp.mutable_response()->set_request_id(req.request_id()); }
    sendPacket(conn, GatewayPacketHelper::makeResponse(MessageType::ACK_RESP, packet.sequence_id, resp));
}

void GatewayRouter::handlePullOfflineMessages(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet) {
    std::optional<ConnectionContext> auth;
    if (!requireAuth(conn, connection_id, packet.sequence_id, &auth)) return;
    proto::PullOfflineMessagesRequest req;
    if (!GatewayPacketHelper::parseBody(packet, &req)) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::GATEWAY_INVALID_PACKET), "bad pull body", ""); return; }
    if (req.user_id() == 0) req.set_user_id(auth->user_id);
    if (req.user_id() != auth->user_id) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::GATEWAY_PERMISSION_DENIED), "permission denied", req.request_id()); return; }
    proto::PullOfflineMessagesResponse resp;
    grpc::ClientContext ctx;
    setDeadline(&ctx);
    grpc::Status status = backend_clients_->messageService()->PullOfflineMessages(&ctx, req, &resp);
    if (!status.ok()) { resp.mutable_response()->set_code(static_cast<int>(ErrorCode::GATEWAY_BACKEND_RPC_FAILED)); resp.mutable_response()->set_message(status.error_message()); resp.mutable_response()->set_request_id(req.request_id()); }
    sendPacket(conn, GatewayPacketHelper::makeResponse(MessageType::PULL_OFFLINE_MSG_RESP, packet.sequence_id, resp));
}

}  // namespace nebula
