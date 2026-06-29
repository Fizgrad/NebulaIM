#include "GatewayServiceImpl.h"

#include "common/error/ErrorCode.h"
#include "common/gateway/GatewayPacketHelper.h"
#include "common/log/Logger.h"
#include "common/net/TcpConnection.h"

namespace nebula {

namespace {
void fill(proto::CommonResponse* response, const std::string& request_id, ErrorCode code, const std::string& message = "") {
    response->set_code(static_cast<int32_t>(code));
    response->set_message(message.empty() ? errorCodeToString(code) : message);
    response->set_request_id(request_id.empty() ? "request-id-empty" : request_id);
}
}

GatewayServiceImpl::GatewayServiceImpl(ConnectionManager* connection_manager, PacketCodec* codec, std::string gateway_id)
    : connection_manager_(connection_manager), codec_(codec), gateway_id_(std::move(gateway_id)) {}

grpc::Status GatewayServiceImpl::DeliverToConnection(grpc::ServerContext*, const proto::DeliverToConnectionRequest* request, proto::DeliverToConnectionResponse* response) {
    if (connection_manager_ == nullptr || codec_ == nullptr || request->user_id() == 0 || request->connection_id().empty()) {
        fill(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT);
        return grpc::Status::OK;
    }
    auto conn = connection_manager_->getConnection(request->connection_id());
    if (!conn) {
        fill(response->mutable_response(), request->request_id(), ErrorCode::GATEWAY_CONNECTION_NOT_FOUND);
        return grpc::Status::OK;
    }
    Packet packet = GatewayPacketHelper::makePushMessage(0, request->message());
    conn->send(codec_->encode(packet));
    fill(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status GatewayServiceImpl::KickUser(grpc::ServerContext*, const proto::KickUserRequest* request, proto::KickUserResponse* response) {
    if (connection_manager_ != nullptr) {
        auto conn = connection_manager_->getConnectionByUserId(request->user_id());
        if (conn) conn->forceClose();
    }
    fill(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status GatewayServiceImpl::GetOnlineStatus(grpc::ServerContext*, const proto::GetOnlineStatusRequest* request, proto::GetOnlineStatusResponse* response) {
    auto ctx = connection_manager_ ? connection_manager_->getContextByUserId(request->user_id()) : std::nullopt;
    fill(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    if (ctx.has_value()) {
        response->set_online(true);
        response->set_gateway_id(gateway_id_);
        response->set_connection_id(ctx->connection_id);
    } else {
        response->set_online(false);
    }
    return grpc::Status::OK;
}

}  // namespace nebula
