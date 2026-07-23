#include "TestDeps.h"
#include "PushServiceContext.h"
#include "common/utils/TimeUtil.h"

#include <cassert>

int main() {
    nebula::PushServiceContext context;
    if (!context.init("config/nebula.conf")) return nebula::tests::skip("test_push_dispatcher", "PushService dependencies are not reachable");
    nebula::proto::MessageData message;
    message.set_message_id(static_cast<uint64_t>(nebula::TimeUtil::nowMs()));
    message.set_conversation_id(1);
    message.set_from_user_id(1);
    message.set_to_user_id(95001);
    message.set_content_type(nebula::proto::MESSAGE_CONTENT_TYPE_TEXT);
    message.set_content("dispatcher offline");
    message.set_status(nebula::proto::MESSAGE_STATUS_SENT);
    message.set_timestamp(nebula::TimeUtil::nowMs());
    assert(context.dispatcher()->pushToUser("dispatcher", 95001, message));
    return 0;
}
