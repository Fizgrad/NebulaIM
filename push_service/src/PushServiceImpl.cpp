#include "PushServiceImpl.h"

#include "common/error/ErrorCode.h"
#include "common/log/Logger.h"
#include "common/rpc/InternalRpcAuth.h"

namespace nebula {

namespace {
void fillResponse(proto::CommonResponse* response, const std::string& request_id, ErrorCode code, const std::string& message = "") {
    response->set_code(static_cast<int32_t>(code));
    response->set_message(message.empty() ? errorCodeToString(code) : message);
    response->set_request_id(request_id.empty() ? "request-id-empty" : request_id);
}

bool requireInternalRpc(grpc::ServerContext* context, const std::string& request_id, proto::CommonResponse* response) {
    if (InternalRpcAuth::instance().authorize(context)) return true;
    fillResponse(response, request_id, ErrorCode::AUTH_FAILED, "internal rpc unauthenticated");
    return false;
}
}

PushServiceImpl::PushServiceImpl(PushDispatcher* dispatcher) : dispatcher_(dispatcher) {}

grpc::Status PushServiceImpl::PushToUser(grpc::ServerContext* context, const proto::PushToUserRequest* request, proto::PushToUserResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
    if (dispatcher_ == nullptr) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->user_id() == 0 || request->message().message_id() == 0) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::PUSH_INVALID_MESSAGE); return grpc::Status::OK; }
    bool ok = dispatcher_->pushToUser(request->request_id(), request->user_id(), request->message());
    fillResponse(response->mutable_response(), request->request_id(), ok ? ErrorCode::OK : ErrorCode::PUSH_FAILED, ok ? "OK" : "PUSH_FAILED");
    return grpc::Status::OK;
}

grpc::Status PushServiceImpl::PushToGroup(grpc::ServerContext* context, const proto::PushToGroupRequest* request, proto::PushToGroupResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
    if (dispatcher_ == nullptr) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->group_id() == 0 || request->message().message_id() == 0) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::PUSH_INVALID_MESSAGE); return grpc::Status::OK; }
    PushResult result = dispatcher_->pushToGroup(request->request_id(), request->group_id(), request->message());
    response->set_success_count(result.success_count);
    response->set_failed_count(result.failed_count);
    fillResponse(response->mutable_response(), request->request_id(), result.failed_count == 0 ? ErrorCode::OK : ErrorCode::PUSH_FAILED, result.failed_count == 0 ? "OK" : "PUSH_FAILED");
    return grpc::Status::OK;
}

}  // namespace nebula
