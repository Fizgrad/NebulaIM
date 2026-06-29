#include "common.pb.h"
#include "message.pb.h"

#include <cassert>
#include <string>

int main() {
    nebula::proto::UserInfo user;
    user.set_user_id(10001);
    user.set_username("alice");
    user.set_nickname("Alice");
    user.set_avatar("avatar.png");
    user.set_created_at(123456);

    nebula::proto::MessageData message;
    message.set_message_id(90001);
    message.set_conversation_id(80001);
    message.set_from_user_id(10001);
    message.set_to_user_id(10002);
    message.set_content_type(nebula::proto::MESSAGE_CONTENT_TYPE_TEXT);
    message.set_content("hello");
    message.set_status(nebula::proto::MESSAGE_STATUS_SENT);
    message.set_timestamp(123456789);

    nebula::proto::SendSingleMessageRequest request;
    request.set_request_id("test-1");
    request.set_from_user_id(10001);
    request.set_to_user_id(10002);
    request.set_content_type(nebula::proto::MESSAGE_CONTENT_TYPE_TEXT);
    request.set_content("hello");
    request.set_client_sequence_id(42);

    std::string data;
    assert(request.SerializeToString(&data));

    nebula::proto::SendSingleMessageRequest decoded;
    assert(decoded.ParseFromString(data));
    assert(decoded.request_id() == "test-1");
    assert(decoded.from_user_id() == 10001);
    assert(decoded.to_user_id() == 10002);
    assert(decoded.content_type() == nebula::proto::MESSAGE_CONTENT_TYPE_TEXT);
    assert(decoded.content() == "hello");
    assert(decoded.client_sequence_id() == 42);

    return 0;
}
