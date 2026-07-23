#include "common/push/GatewayClient.h"

#include "common/log/Logger.h"
#include "common/rpc/GrpcClient.h"
#include "common/rpc/InternalRpcAuth.h"
#include "common/rpc/RpcMetadata.h"
#include "common/trace/TraceContext.h"

#include <grpcpp/grpcpp.h>

namespace nebula {

GatewayClient::GatewayClient(const std::string& address)
    : GatewayClient(address, GrpcTlsConfig{}) {}

GatewayClient::GatewayClient(const std::string& address, const GrpcTlsConfig& tls_config) {
    auto channel = GrpcClient::createChannel(address, tls_config);
    if (channel) {
        stub_ = proto::GatewayService::NewStub(channel);
    }
}

bool GatewayClient::deliverToConnection(const std::string& request_id, uint64_t user_id, const std::string& connection_id, const proto::MessageData& message) {
    if (!stub_) return false;
    if (!breaker_.allowRequest()) return false;
    proto::DeliverToConnectionRequest req;
    req.set_request_id(request_id);
    req.set_user_id(user_id);
    req.set_connection_id(connection_id);
    *req.mutable_message() = message;
    proto::DeliverToConnectionResponse resp;
    grpc::ClientContext ctx;
    RpcMetadata::injectTraceId(&ctx, TraceContext::ensureTraceId(request_id));
    InternalRpcAuth::instance().inject(&ctx);
    grpc::Status status = stub_->DeliverToConnection(&ctx, req, &resp);
    if (!status.ok() || resp.response().code() != 0) {
        breaker_.recordFailure();
        LOG_ERROR("Gateway DeliverToConnection failed: " + status.error_message());
        return false;
    }
    breaker_.recordSuccess();
    return true;
}

bool GatewayClient::kickUser(const std::string& request_id, uint64_t user_id, const std::string& reason) {
    if (!stub_) return false;
    if (!breaker_.allowRequest()) return false;
    proto::KickUserRequest req;
    req.set_request_id(request_id);
    req.set_user_id(user_id);
    req.set_reason(reason);
    proto::KickUserResponse resp;
    grpc::ClientContext ctx;
    RpcMetadata::injectTraceId(&ctx, TraceContext::ensureTraceId(request_id));
    InternalRpcAuth::instance().inject(&ctx);
    grpc::Status status = stub_->KickUser(&ctx, req, &resp);
    bool ok = status.ok() && resp.response().code() == 0;
    if (ok) breaker_.recordSuccess();
    else breaker_.recordFailure();
    return ok;
}

bool GatewayClient::kickConnection(const std::string& request_id,
                                   uint64_t user_id,
                                   const std::string& connection_id,
                                   const std::string& device_id,
                                   const std::string& reason) {
    if (!stub_) return false;
    if (!breaker_.allowRequest()) return false;
    proto::KickConnectionRequest req;
    req.set_request_id(request_id);
    req.set_user_id(user_id);
    req.set_connection_id(connection_id);
    req.set_device_id(device_id);
    req.set_reason(reason);
    proto::KickConnectionResponse resp;
    grpc::ClientContext ctx;
    RpcMetadata::injectTraceId(&ctx, TraceContext::ensureTraceId(request_id));
    InternalRpcAuth::instance().inject(&ctx);
    grpc::Status status = stub_->KickConnection(&ctx, req, &resp);
    bool ok = status.ok() && resp.response().code() == 0;
    if (ok) breaker_.recordSuccess();
    else breaker_.recordFailure();
    return ok;
}

bool GatewayClient::getOnlineStatus(const std::string& request_id, uint64_t user_id, bool* online, std::string* gateway_id, std::string* connection_id) {
    if (!stub_) return false;
    if (!breaker_.allowRequest()) return false;
    proto::GetOnlineStatusRequest req;
    req.set_request_id(request_id);
    req.set_user_id(user_id);
    proto::GetOnlineStatusResponse resp;
    grpc::ClientContext ctx;
    RpcMetadata::injectTraceId(&ctx, TraceContext::ensureTraceId(request_id));
    InternalRpcAuth::instance().inject(&ctx);
    grpc::Status status = stub_->GetOnlineStatus(&ctx, req, &resp);
    if (!status.ok() || resp.response().code() != 0) {
        breaker_.recordFailure();
        return false;
    }
    breaker_.recordSuccess();
    if (online) *online = resp.online();
    if (gateway_id) *gateway_id = resp.gateway_id();
    if (connection_id) *connection_id = resp.connection_id();
    return true;
}

}  // namespace nebula
