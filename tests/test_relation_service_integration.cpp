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
    assert(context.init("config/nebula.conf"));
    nebula::RelationServiceImpl service(context.userDao(), context.relationDao(), context.groupDao());
    grpc::ServerContext server_context;

    uint64_t u1 = createUser(context.userDao(), "rsi_u1_");
    uint64_t u2 = createUser(context.userDao(), "rsi_u2_");

    nebula::proto::CommonResponse add_resp;
    nebula::proto::AddFriendRequest add_req;
    add_req.set_request_id("integration-add");
    add_req.set_user_id(u1);
    add_req.set_friend_id(u2);
    assert(service.AddFriend(&server_context, &add_req, &add_resp).ok());
    assert(add_resp.code() == 0);

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
