#include "PushServiceContext.h"
#include "PushServiceImpl.h"
#include "common/utils/TimeUtil.h"

#include <cassert>
#include <grpcpp/grpcpp.h>

int main() {
    nebula::PushServiceContext context;
    assert(context.init("config/nebula.conf"));
    nebula::PushServiceImpl service(context.dispatcher());
    grpc::ServerContext server_context;

    nebula::proto::MessageData message;
    message.set_message_id(static_cast<uint64_t>(nebula::TimeUtil::nowMs()));
    message.set_conversation_id(1);
    message.set_from_user_id(1);
    message.set_to_user_id(93001);
    message.set_content_type(nebula::proto::MESSAGE_CONTENT_TYPE_TEXT);
    message.set_content("push impl");
    message.set_status(nebula::proto::MESSAGE_STATUS_SENT);
    message.set_timestamp(nebula::TimeUtil::nowMs());

    nebula::proto::PushToUserRequest req;
    req.set_request_id("push-user");
    req.set_user_id(93001);
    *req.mutable_message() = message;
    nebula::proto::PushToUserResponse resp;
    assert(service.PushToUser(&server_context, &req, &resp).ok());
    assert(resp.response().code() == 0);

    nebula::proto::PushToUserRequest bad_req;
    bad_req.set_request_id("bad");
    bad_req.set_user_id(0);
    *bad_req.mutable_message() = message;
    nebula::proto::PushToUserResponse bad_resp;
    assert(service.PushToUser(&server_context, &bad_req, &bad_resp).ok());
    assert(bad_resp.response().code() != 0);
    return 0;
}
