#include "PushServiceImpl.h"

#include "common/error/ErrorCode.h"
#include "common/log/Logger.h"

namespace nebula {

namespace {
void fillResponse(proto::CommonResponse* response, const std::string& request_id, ErrorCode code, const std::string& message = "") {
    response->set_code(static_cast<int32_t>(code));
    response->set_message(message.empty() ? errorCodeToString(code) : message);
    response->set_request_id(request_id.empty() ? "request-id-empty" : request_id);
}
}

PushServiceImpl::PushServiceImpl(PushDispatcher* dispatcher) : dispatcher_(dispatcher) {}

grpc::Status PushServiceImpl::PushToUser(grpc::ServerContext*, const proto::PushToUserRequest* request, proto::PushToUserResponse* response) {
    if (dispatcher_ == nullptr) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->user_id() == 0 || request->message().message_id() == 0) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::PUSH_INVALID_MESSAGE); return grpc::Status::OK; }
    bool ok = dispatcher_->pushToUser(request->request_id(), request->user_id(), request->message());
    fillResponse(response->mutable_response(), request->request_id(), ok ? ErrorCode::OK : ErrorCode::PUSH_FAILED, ok ? "OK" : "PUSH_FAILED");
    return grpc::Status::OK;
}

grpc::Status PushServiceImpl::PushToGroup(grpc::ServerContext*, const proto::PushToGroupRequest* request, proto::PushToGroupResponse* response) {
    if (dispatcher_ == nullptr) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->group_id() == 0 || request->message().message_id() == 0) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::PUSH_INVALID_MESSAGE); return grpc::Status::OK; }
    PushResult result = dispatcher_->pushToGroup(request->request_id(), request->group_id(), request->message());
    response->set_success_count(result.success_count);
    response->set_failed_count(result.failed_count);
    fillResponse(response->mutable_response(), request->request_id(), result.failed_count == 0 ? ErrorCode::OK : ErrorCode::PUSH_FAILED, result.failed_count == 0 ? "OK" : "PUSH_FAILED");
    return grpc::Status::OK;
}

}  // namespace nebula
