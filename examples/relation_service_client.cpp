#include "common/auth/PasswordHasher.h"
#include "common/config/Config.h"
#include "common/dao/UserDao.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/utils/TimeUtil.h"
#include "relation.grpc.pb.h"

#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <string>

namespace {
std::string parseAddress(int argc, char** argv) {
    std::string address = "127.0.0.1:50053";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--addr" && i + 1 < argc) address = argv[++i];
    }
    return address;
}

uint64_t createUser(nebula::UserDao& dao, const std::string& prefix) {
    int64_t now = nebula::TimeUtil::nowMs();
    nebula::User user;
    user.username = prefix + std::to_string(now);
    user.password_hash = nebula::PasswordHasher::hashPassword("password123");
    user.nickname = prefix;
    user.created_at = now;
    user.updated_at = now;
    uint64_t id = 0;
    dao.createUser(user, &id);
    return id;
}
}

int main(int argc, char** argv) {
    nebula::Config config;
    if (!config.loadFromFile("config/nebula.conf")) {
        std::cerr << "failed to load config/nebula.conf" << std::endl;
        return 1;
    }
    nebula::MySqlConfig mysql;
    mysql.host = config.getString("mysql.host", mysql.host);
    mysql.port = config.getInt("mysql.port", mysql.port);
    mysql.user = config.getString("mysql.user", mysql.user);
    mysql.password = config.getString("mysql.password", mysql.password);
    mysql.database = config.getString("mysql.database", mysql.database);
    nebula::MySqlConnectionPool pool;
    if (!pool.init(mysql, 2)) {
        std::cerr << "failed to initialize MySQL pool" << std::endl;
        return 1;
    }
    nebula::UserDao user_dao(pool);
    uint64_t u1 = createUser(user_dao, "rel_client_u1_");
    uint64_t u2 = createUser(user_dao, "rel_client_u2_");

    auto stub = nebula::proto::RelationService::NewStub(grpc::CreateChannel(parseAddress(argc, argv), grpc::InsecureChannelCredentials()));

    nebula::proto::SendFriendRequestRequest friend_req;
    friend_req.set_request_id("client-friend-request");
    friend_req.set_from_user_id(u1);
    friend_req.set_to_user_id(u2);
    friend_req.set_message("hello");
    nebula::proto::SendFriendRequestResponse friend_resp;
    grpc::ClientContext friend_ctx;
    stub->SendFriendRequest(&friend_ctx, friend_req, &friend_resp);
    std::cout << "SendFriendRequest code=" << friend_resp.response().code()
              << " request_id=" << friend_resp.friend_request_id() << std::endl;

    nebula::proto::AcceptFriendRequestRequest accept_req;
    accept_req.set_request_id("client-friend-accept");
    accept_req.set_user_id(u2);
    accept_req.set_friend_request_id(friend_resp.friend_request_id());
    nebula::proto::CommonResponse accept_resp;
    grpc::ClientContext accept_ctx;
    stub->AcceptFriendRequest(&accept_ctx, accept_req, &accept_resp);
    std::cout << "AcceptFriendRequest code=" << accept_resp.code() << std::endl;

    nebula::proto::ListFriendsRequest list_req;
    list_req.set_request_id("client-list");
    list_req.set_user_id(u1);
    nebula::proto::ListFriendsResponse list_resp;
    grpc::ClientContext list_ctx;
    stub->ListFriends(&list_ctx, list_req, &list_resp);
    std::cout << "ListFriends count=" << list_resp.friends_size() << std::endl;

    nebula::proto::CreateGroupRequest group_req;
    group_req.set_request_id("client-group");
    group_req.set_owner_id(u1);
    group_req.set_group_name("client_group");
    nebula::proto::CreateGroupResponse group_resp;
    grpc::ClientContext group_ctx;
    stub->CreateGroup(&group_ctx, group_req, &group_resp);
    std::cout << "CreateGroup code=" << group_resp.response().code() << " group_id=" << group_resp.group_id() << std::endl;

    nebula::proto::JoinGroupRequest join_req;
    join_req.set_request_id("client-join");
    join_req.set_user_id(u2);
    join_req.set_group_id(group_resp.group_id());
    nebula::proto::CommonResponse join_resp;
    grpc::ClientContext join_ctx;
    stub->JoinGroup(&join_ctx, join_req, &join_resp);
    std::cout << "JoinGroup code=" << join_resp.code() << std::endl;

    nebula::proto::ListGroupMembersRequest members_req;
    members_req.set_request_id("client-members");
    members_req.set_group_id(group_resp.group_id());
    nebula::proto::ListGroupMembersResponse members_resp;
    grpc::ClientContext members_ctx;
    stub->ListGroupMembers(&members_ctx, members_req, &members_resp);
    std::cout << "ListGroupMembers count=" << members_resp.members_size() << std::endl;

    nebula::proto::LeaveGroupRequest leave_req;
    leave_req.set_request_id("client-leave");
    leave_req.set_user_id(u2);
    leave_req.set_group_id(group_resp.group_id());
    nebula::proto::CommonResponse leave_resp;
    grpc::ClientContext leave_ctx;
    stub->LeaveGroup(&leave_ctx, leave_req, &leave_resp);
    std::cout << "LeaveGroup code=" << leave_resp.code() << std::endl;

    nebula::proto::DeleteFriendRequest del_req;
    del_req.set_request_id("client-del");
    del_req.set_user_id(u1);
    del_req.set_friend_id(u2);
    nebula::proto::CommonResponse del_resp;
    grpc::ClientContext del_ctx;
    stub->DeleteFriend(&del_ctx, del_req, &del_resp);
    std::cout << "DeleteFriend code=" << del_resp.code() << std::endl;
    return 0;
}
