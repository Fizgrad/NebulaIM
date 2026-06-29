#include "common/push/GatewayClient.h"

#include "common/log/Logger.h"

#include <grpcpp/grpcpp.h>

namespace nebula {

GatewayClient::GatewayClient(const std::string& address)
    : stub_(proto::GatewayService::NewStub(grpc::CreateChannel(address, grpc::InsecureChannelCredentials()))) {}

bool GatewayClient::deliverToConnection(const std::string& request_id, uint64_t user_id, const std::string& connection_id, const proto::MessageData& message) {
    proto::DeliverToConnectionRequest req;
    req.set_request_id(request_id);
    req.set_user_id(user_id);
    req.set_connection_id(connection_id);
    *req.mutable_message() = message;
    proto::DeliverToConnectionResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub_->DeliverToConnection(&ctx, req, &resp);
    if (!status.ok() || resp.response().code() != 0) {
        LOG_ERROR("Gateway DeliverToConnection failed: " + status.error_message());
        return false;
    }
    return true;
}

bool GatewayClient::kickUser(const std::string& request_id, uint64_t user_id, const std::string& reason) {
    proto::KickUserRequest req;
    req.set_request_id(request_id);
    req.set_user_id(user_id);
    req.set_reason(reason);
    proto::KickUserResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub_->KickUser(&ctx, req, &resp);
    return status.ok() && resp.response().code() == 0;
}

bool GatewayClient::getOnlineStatus(const std::string& request_id, uint64_t user_id, bool* online, std::string* gateway_id, std::string* connection_id) {
    proto::GetOnlineStatusRequest req;
    req.set_request_id(request_id);
    req.set_user_id(user_id);
    proto::GetOnlineStatusResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub_->GetOnlineStatus(&ctx, req, &resp);
    if (!status.ok() || resp.response().code() != 0) return false;
    if (online) *online = resp.online();
    if (gateway_id) *gateway_id = resp.gateway_id();
    if (connection_id) *connection_id = resp.connection_id();
    return true;
}

}  // namespace nebula
