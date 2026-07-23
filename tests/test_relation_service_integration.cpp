#include "TestDeps.h"
#include "RelationServiceContext.h"
#include "RelationServiceImpl.h"
#include "common/auth/PasswordHasher.h"
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
    nebula::RelationServiceContext context;
    if (!context.init("config/nebula.conf")) return nebula::tests::skip("test_relation_service_integration", "RelationService dependencies are not reachable");
    nebula::RelationServiceImpl service(context.userDao(), context.relationDao(), context.groupDao(), context.friendRequestDao());
    grpc::ServerContext server_context;

    uint64_t u1 = createUser(context.userDao(), "rsi_u1_");
    uint64_t u2 = createUser(context.userDao(), "rsi_u2_");

    nebula::proto::SendFriendRequestRequest friend_req;
    friend_req.set_request_id("integration-friend-request");
    friend_req.set_from_user_id(u1);
    friend_req.set_to_user_id(u2);
    nebula::proto::SendFriendRequestResponse friend_resp;
    assert(service.SendFriendRequest(&server_context, &friend_req, &friend_resp).ok());
    assert(friend_resp.response().code() == 0);

    nebula::proto::AcceptFriendRequestRequest accept_req;
    accept_req.set_request_id("integration-friend-accept");
    accept_req.set_user_id(u2);
    accept_req.set_friend_request_id(friend_resp.friend_request_id());
    nebula::proto::CommonResponse accept_resp;
    assert(service.AcceptFriendRequest(&server_context, &accept_req, &accept_resp).ok());
    assert(accept_resp.code() == 0);

    nebula::proto::CreateGroupRequest group_req;
    group_req.set_request_id("integration-group");
    group_req.set_owner_id(u1);
    group_req.set_group_name("integration_group");
    nebula::proto::CreateGroupResponse group_resp;
    assert(service.CreateGroup(&server_context, &group_req, &group_resp).ok());
    assert(group_resp.response().code() == 0);
    assert(group_resp.group_id() > 0);

    return 0;
}
