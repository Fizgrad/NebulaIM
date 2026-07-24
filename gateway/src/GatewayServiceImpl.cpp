#include "GatewayServiceImpl.h"

#include "common/error/ErrorCode.h"
#include "common/gateway/GatewayPacketHelper.h"
#include "common/log/Logger.h"
#include "common/net/TcpConnection.h"
#include "common/rpc/InternalRpcAuth.h"
#include "common/websocket/WebSocketCodec.h"

#include <vector>

namespace nebula {

namespace {
void fill(proto::CommonResponse* response, const std::string& request_id, ErrorCode code, const std::string& message = "") {
    response->set_code(static_cast<int32_t>(code));
    response->set_message(message.empty() ? errorCodeToString(code) : message);
    response->set_request_id(request_id.empty() ? "request-id-empty" : request_id);
}

bool requireInternalRpc(grpc::ServerContext* context, const std::string& request_id, proto::CommonResponse* response) {
    if (InternalRpcAuth::instance().authorize(context)) return true;
    fill(response, request_id, ErrorCode::AUTH_FAILED, "internal rpc unauthenticated");
    return false;
}
}

GatewayServiceImpl::GatewayServiceImpl(ConnectionManager* connection_manager,
                                       GatewayOnlineManager* online_manager,
                                       PacketCodec* codec)
    : connection_manager_(connection_manager), online_manager_(online_manager), codec_(codec) {}

grpc::Status GatewayServiceImpl::DeliverToConnection(grpc::ServerContext* context, const proto::DeliverToConnectionRequest* request, proto::DeliverToConnectionResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
    if (connection_manager_ == nullptr || codec_ == nullptr || request->user_id() == 0 || request->connection_id().empty()) {
        fill(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT);
        return grpc::Status::OK;
    }
    auto conn = connection_manager_->getConnection(request->connection_id());
    if (!conn || !conn->connected()) {
        fill(response->mutable_response(), request->request_id(), ErrorCode::GATEWAY_CONNECTION_NOT_FOUND);
        return grpc::Status::OK;
    }
    Packet packet = GatewayPacketHelper::makePushMessage(0, request->message());
    std::string payload = codec_->encode(packet);
    auto ctx = connection_manager_->getContext(request->connection_id());
    if (!ctx.has_value() || ctx->user_id != request->user_id()) {
        fill(response->mutable_response(), request->request_id(), ErrorCode::GATEWAY_CONNECTION_NOT_FOUND, "connection user mismatch");
        return grpc::Status::OK;
    }
    LOG_INFO("gateway deliver connection request_id=" + request->request_id() +
             " user_id=" + std::to_string(request->user_id()) +
             " connection_id=" + request->connection_id() +
             " websocket=" + (ctx->websocket ? "true" : "false") +
             " payload_bytes=" + std::to_string(payload.size()));
    if (ctx.has_value() && ctx->websocket) {
        WebSocketCodec websocket_codec;
        conn->send(websocket_codec.encodeBinary(payload));
    } else {
        conn->send(payload);
    }
    fill(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status GatewayServiceImpl::KickUser(grpc::ServerContext* context, const proto::KickUserRequest* request, proto::KickUserResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
    if (connection_manager_ != nullptr) {
        auto conns = connection_manager_->getConnectionsByUserId(request->user_id());
        for (const auto& conn : conns) {
            if (conn) conn->forceClose();
        }
    }
    fill(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status GatewayServiceImpl::KickConnection(grpc::ServerContext* context, const proto::KickConnectionRequest* request, proto::KickConnectionResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
    if (connection_manager_ == nullptr || request->user_id() == 0 || request->connection_id().empty()) {
        fill(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT);
        return grpc::Status::OK;
    }
    auto ctx = connection_manager_->getContext(request->connection_id());
    if (!ctx.has_value() || ctx->user_id != request->user_id()) {
        fill(response->mutable_response(), request->request_id(), ErrorCode::GATEWAY_CONNECTION_NOT_FOUND);
        return grpc::Status::OK;
    }
    if (!request->device_id().empty() && ctx->device_id != request->device_id()) {
        fill(response->mutable_response(), request->request_id(), ErrorCode::GATEWAY_CONNECTION_NOT_FOUND, "connection device mismatch");
        return grpc::Status::OK;
    }
    auto conn = connection_manager_->getConnection(request->connection_id());
    if (!conn || !conn->connected()) {
        fill(response->mutable_response(), request->request_id(), ErrorCode::GATEWAY_CONNECTION_NOT_FOUND);
        return grpc::Status::OK;
    }
    LOG_INFO("gateway kick connection request_id=" + request->request_id() +
             " user_id=" + std::to_string(request->user_id()) +
             " device_id=" + request->device_id() +
             " connection_id=" + request->connection_id() +
             " reason=" + request->reason());
    conn->forceClose();
    fill(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status GatewayServiceImpl::GetOnlineStatus(grpc::ServerContext* context, const proto::GetOnlineStatusRequest* request, proto::GetOnlineStatusResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
    if (request->user_id() == 0 || online_manager_ == nullptr) {
        fill(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT);
        return grpc::Status::OK;
    }
    const auto location = online_manager_->findOnline(request->user_id());
    fill(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    if (location.has_value()) {
        response->set_online(true);
        response->set_gateway_id(location->gateway_id);
        response->set_connection_id(location->connection_id);
    } else {
        response->set_online(false);
    }
    return grpc::Status::OK;
}

}  // namespace nebula
