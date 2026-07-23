#include "TestDeps.h"
#include "MessageServiceContext.h"
#include "MessageServiceImpl.h"
#include "common/auth/PasswordHasher.h"
#include "common/dao/RelationDao.h"
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
    if (!context.init("config/nebula.conf")) return nebula::tests::skip("test_message_service_integration", "MessageService dependencies are not reachable");
    nebula::MessageServiceImpl service(context.userDao(), context.groupDao(), context.relationDao(), context.messageDao(), context.offlineMessageDao(), context.redisClient(), context.kafkaProducer(), context.messageIdGenerator(), context.messageDeduplicator(), context.options(), context.mysqlPool(), context.conversationDao(), context.messageReceiptDao(), context.outboxDao());
    grpc::ServerContext server_context;
    uint64_t u1 = createUser(context.userDao(), "msi_u1_");
    uint64_t u2 = createUser(context.userDao(), "msi_u2_");
    assert(context.relationDao()->addFriendBidirectional(u1, u2));

    nebula::proto::SendSingleMessageRequest req;
    req.set_request_id("integration-single");
    req.set_from_user_id(u1);
    req.set_to_user_id(u2);
    req.set_content_type(nebula::proto::MESSAGE_CONTENT_TYPE_TEXT);
    req.set_content("integration hello");
    req.set_client_sequence_id(909);
    nebula::proto::SendSingleMessageResponse resp;
    assert(service.SendSingleMessage(&server_context, &req, &resp).ok());
    assert(resp.response().code() == 0);
    assert(resp.message_id() > 0);
    return 0;
}
