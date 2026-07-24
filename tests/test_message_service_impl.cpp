#include "TestDeps.h"
#include "MessageServiceContext.h"
#include "MessageServiceImpl.h"
#include "common/auth/PasswordHasher.h"
#include "common/dao/GroupDao.h"
#include "common/dao/MessageDao.h"
#include "common/dao/MessageReceiptDao.h"
#include "common/dao/OfflineMessageDao.h"
#include "common/dao/RelationDao.h"
#include "common/dao/UserDao.h"
#include "common/error/ErrorCode.h"
#include "common/message/MessageKafkaPayload.h"
#include "common/utils/TimeUtil.h"

#include <cassert>
#include <cstdlib>
#include <grpcpp/grpcpp.h>

namespace {
uint64_t createUser(nebula::UserDao* dao, const std::string& prefix) {
    int64_t now = nebula::TimeUtil::nowMs();
    nebula::User user;
    user.username = prefix + std::to_string(now);
    user.password_hash = nebula::PasswordHasher::hashPassword("password123");
    user.nickname = prefix;
    user.created_at = now;
    user.updated_at = now;
    uint64_t id = 0;
    assert(dao->createUser(user, &id));
    return id;
}
}

int main() {
    const char* config_path = std::getenv("NEBULA_CONFIG");
    nebula::MessageServiceContext context;
    if (!context.init(config_path != nullptr ? config_path : "config/nebula.conf")) return nebula::tests::skip("test_message_service_impl", "MessageService dependencies are not reachable");
    nebula::MessageServiceImpl service(context.userDao(), context.groupDao(), context.relationDao(), context.messageDao(), context.offlineMessageDao(), context.redisClient(), context.messageIdGenerator(), context.messageDeduplicator(), context.options(), context.mysqlPool(), context.conversationDao(), context.messageReceiptDao(), context.outboxDao());
    grpc::ServerContext server_context;

    uint64_t user1 = createUser(context.userDao(), "msg_u1_");
    uint64_t user2 = createUser(context.userDao(), "msg_u2_");
    uint64_t user3 = createUser(context.userDao(), "msg_u3_");
    assert(context.relationDao()->addFriendBidirectional(user1, user2));

    nebula::proto::SendSingleMessageRequest single;
    single.set_request_id("single");
    single.set_from_user_id(user1);
    single.set_to_user_id(user2);
    single.set_content_type(nebula::proto::MESSAGE_CONTENT_TYPE_TEXT);
    single.set_content("hello single");
    single.set_client_sequence_id(101);
    nebula::proto::SendSingleMessageResponse single_resp;
    assert(service.SendSingleMessage(&server_context, &single, &single_resp).ok());
    assert(single_resp.response().code() == 0);
    assert(single_resp.message_id() > 0);
    auto stored = context.messageDao()->getMessageById(single_resp.message_id());
    assert(stored.has_value());
    assert(stored->content == "hello single");
    assert(stored->message_type == static_cast<int>(nebula::proto::MESSAGE_CONTENT_TYPE_TEXT));

    nebula::proto::SendSingleMessageResponse single_dup;
    assert(service.SendSingleMessage(&server_context, &single, &single_dup).ok());
    assert(single_dup.response().code() == 0);
    assert(single_dup.message_id() == single_resp.message_id());

    nebula::proto::SendSingleMessageRequest image_single = single;
    image_single.set_request_id("single-image");
    image_single.set_content_type(nebula::proto::MESSAGE_CONTENT_TYPE_IMAGE);
    image_single.set_content("https://example.com/uploads/images/demo.png");
    image_single.set_client_sequence_id(102);
    nebula::proto::SendSingleMessageResponse image_resp;
    assert(service.SendSingleMessage(&server_context, &image_single, &image_resp).ok());
    assert(image_resp.response().code() == 0);
    auto image_stored = context.messageDao()->getMessageById(image_resp.message_id());
    assert(image_stored.has_value());
    assert(image_stored->message_type == static_cast<int>(nebula::proto::MESSAGE_CONTENT_TYPE_IMAGE));

    int64_t now = nebula::TimeUtil::nowMs();
    nebula::Group group;
    group.group_name = "msg_group_" + std::to_string(now);
    group.owner_id = user1;
    group.created_at = now;
    group.updated_at = now;
    uint64_t group_id = 0;
    assert(context.groupDao()->createGroup(group, &group_id));

    nebula::proto::SendGroupMessageRequest group_req;
    group_req.set_request_id("group");
    group_req.set_from_user_id(user1);
    group_req.set_group_id(group_id);
    group_req.set_content_type(nebula::proto::MESSAGE_CONTENT_TYPE_TEXT);
    group_req.set_content("hello group");
    group_req.set_client_sequence_id(201);
    nebula::proto::SendGroupMessageResponse group_resp;
    assert(service.SendGroupMessage(&server_context, &group_req, &group_resp).ok());
    assert(group_resp.response().code() == 0);
    assert(context.messageDao()->getMessageById(group_resp.message_id()).has_value());

    nebula::proto::SendGroupMessageRequest denied = group_req;
    denied.set_request_id("denied");
    denied.set_from_user_id(user3);
    denied.set_client_sequence_id(202);
    nebula::proto::SendGroupMessageResponse denied_resp;
    assert(service.SendGroupMessage(&server_context, &denied, &denied_resp).ok());
    assert(denied_resp.response().code() == static_cast<int>(nebula::ErrorCode::MESSAGE_PERMISSION_DENIED));

    nebula::proto::AckMessageRequest ack;
    ack.set_request_id("ack");
    ack.set_user_id(user2);
    ack.set_message_id(single_resp.message_id());
    nebula::proto::AckMessageResponse ack_resp;
    assert(service.AckMessage(&server_context, &ack, &ack_resp).ok());
    assert(ack_resp.response().code() == 0);
    assert(context.messageDao()->getMessageById(single_resp.message_id())->status == nebula::proto::MESSAGE_STATUS_SENT);
    const auto receipt_states = context.messageReceiptDao()->getReadState(single_resp.message_id());
    bool delivered_to_recipient = false;
    for (const auto& state : receipt_states) {
        if (state.user_id == user2 && state.delivered_at > 0 && state.read_at == 0) {
            delivered_to_recipient = true;
        }
    }
    assert(delivered_to_recipient);

    nebula::proto::AckMessageRequest unauthorized_ack = ack;
    unauthorized_ack.set_request_id("ack-denied");
    unauthorized_ack.set_user_id(user3);
    nebula::proto::AckMessageResponse unauthorized_ack_resp;
    assert(service.AckMessage(&server_context, &unauthorized_ack, &unauthorized_ack_resp).ok());
    assert(unauthorized_ack_resp.response().code() == static_cast<int>(nebula::ErrorCode::MESSAGE_PERMISSION_DENIED));

    nebula::proto::MessageData data;
    data.set_message_id(777001);
    data.set_conversation_id(888001);
    data.set_from_user_id(user1);
    data.set_to_user_id(user2);
    data.set_content_type(nebula::proto::MESSAGE_CONTENT_TYPE_TEXT);
    data.set_content("offline");
    data.set_status(nebula::proto::MESSAGE_STATUS_SENT);
    data.set_timestamp(nebula::TimeUtil::nowMs());
    nebula::OfflineMessage offline;
    offline.user_id = user2;
    offline.message_id = data.message_id();
    offline.payload = nebula::MessageKafkaPayload::serializeMessageData(data);
    offline.created_at = data.timestamp();
    offline.updated_at = data.timestamp();
    assert(context.offlineMessageDao()->insertOfflineMessage(offline));

    nebula::proto::PullOfflineMessagesRequest pull;
    pull.set_request_id("pull");
    pull.set_user_id(user2);
    pull.mutable_page()->set_page(1);
    pull.mutable_page()->set_page_size(10);
    nebula::proto::PullOfflineMessagesResponse pull_resp;
    assert(service.PullOfflineMessages(&server_context, &pull, &pull_resp).ok());
    assert(pull_resp.response().code() == 0);
    assert(pull_resp.messages_size() >= 1);
    assert(context.offlineMessageDao()->markAsAcked(user2, data.message_id()));
    offline.payload = "duplicate-pending-payload";
    offline.status = static_cast<int>(nebula::OfflineMessageStatus::Pending);
    offline.updated_at = nebula::TimeUtil::nowMs();
    assert(context.offlineMessageDao()->insertOfflineMessage(offline));
    auto after_ack_duplicate = context.offlineMessageDao()->listOfflineMessages(user2, 50);
    for (const auto& item : after_ack_duplicate) {
        assert(item.message_id != data.message_id());
    }

    nebula::proto::SendSingleMessageRequest bad_user = single;
    bad_user.set_request_id("bad-user");
    bad_user.set_from_user_id(999999999);
    bad_user.set_client_sequence_id(301);
    nebula::proto::SendSingleMessageResponse bad_user_resp;
    assert(service.SendSingleMessage(&server_context, &bad_user, &bad_user_resp).ok());
    assert(bad_user_resp.response().code() == static_cast<int>(nebula::ErrorCode::USER_NOT_FOUND));

    nebula::proto::SendSingleMessageRequest empty = single;
    empty.set_request_id("empty");
    empty.set_content("");
    empty.set_client_sequence_id(302);
    nebula::proto::SendSingleMessageResponse empty_resp;
    assert(service.SendSingleMessage(&server_context, &empty, &empty_resp).ok());
    assert(empty_resp.response().code() == static_cast<int>(nebula::ErrorCode::MESSAGE_EMPTY));

    nebula::proto::SendSingleMessageRequest large = single;
    large.set_request_id("large");
    large.set_content(std::string(static_cast<size_t>(context.options().max_content_length + 1), 'x'));
    large.set_client_sequence_id(303);
    nebula::proto::SendSingleMessageResponse large_resp;
    assert(service.SendSingleMessage(&server_context, &large, &large_resp).ok());
    assert(large_resp.response().code() == static_cast<int>(nebula::ErrorCode::MESSAGE_TOO_LARGE));

    return 0;
}
