#include "MessageServiceContext.h"
#include "MessageServiceImpl.h"
#include "common/auth/PasswordHasher.h"
#include "common/dao/GroupDao.h"
#include "common/dao/MessageDao.h"
#include "common/dao/OfflineMessageDao.h"
#include "common/dao/UserDao.h"
#include "common/error/ErrorCode.h"
#include "common/message/MessageKafkaPayload.h"
#include "common/utils/TimeUtil.h"

#include <cassert>
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
    nebula::MessageServiceContext context;
    assert(context.init("config/nebula.conf"));
    nebula::MessageServiceImpl service(context.userDao(), context.groupDao(), context.messageDao(), context.offlineMessageDao(), context.redisClient(), context.kafkaProducer(), context.messageIdGenerator(), context.messageDeduplicator(), context.options());
    grpc::ServerContext server_context;

    uint64_t user1 = createUser(context.userDao(), "msg_u1_");
    uint64_t user2 = createUser(context.userDao(), "msg_u2_");
    uint64_t user3 = createUser(context.userDao(), "msg_u3_");

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

    nebula::proto::SendSingleMessageResponse single_dup;
    assert(service.SendSingleMessage(&server_context, &single, &single_dup).ok());
    assert(single_dup.response().code() == 0);
    assert(single_dup.message_id() == single_resp.message_id());

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
    assert(context.messageDao()->getMessageById(single_resp.message_id())->status == nebula::proto::MESSAGE_STATUS_ACKED);

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
