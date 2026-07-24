#include "TestDeps.h"
#include "RelationServiceContext.h"
#include "RelationServiceImpl.h"
#include "common/auth/PasswordHasher.h"
#include "common/error/ErrorCode.h"
#include "common/utils/TimeUtil.h"

#include <algorithm>
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
    if (!context.init("config/nebula.conf")) return nebula::tests::skip("test_relation_service_impl", "RelationService dependencies are not reachable");
    nebula::RelationServiceImpl service(context.userDao(), context.relationDao(), context.groupDao(), context.friendRequestDao());
    grpc::ServerContext server_context;

    uint64_t user1 = createUser(context.userDao(), "rs_u1_");
    uint64_t user2 = createUser(context.userDao(), "rs_u2_");
    uint64_t user3 = createUser(context.userDao(), "rs_u3_");

    nebula::proto::CommonResponse common;
    nebula::proto::AddFriendRequest add;
    add.set_request_id("add");
    add.set_user_id(user1);
    add.set_friend_id(user2);
    assert(service.AddFriend(&server_context, &add, &common).ok());
    assert(common.code() == static_cast<int>(nebula::ErrorCode::FRIEND_REQUEST_REQUIRED));

    nebula::proto::SendFriendRequestRequest friend_req;
    friend_req.set_request_id("friend-request");
    friend_req.set_from_user_id(user1);
    friend_req.set_to_user_id(user2);
    friend_req.set_message("hello");
    nebula::proto::SendFriendRequestResponse friend_resp;
    assert(service.SendFriendRequest(&server_context, &friend_req, &friend_resp).ok());
    assert(friend_resp.response().code() == 0);
    assert(friend_resp.friend_request_id() > 0);

    nebula::proto::AcceptFriendRequestRequest accept_req;
    accept_req.set_request_id("friend-accept");
    accept_req.set_user_id(user2);
    accept_req.set_friend_request_id(friend_resp.friend_request_id());
    nebula::proto::CommonResponse accept_resp;
    assert(service.AcceptFriendRequest(&server_context, &accept_req, &accept_resp).ok());
    assert(accept_resp.code() == 0);

    nebula::proto::ListFriendsRequest list_req;
    list_req.set_request_id("list");
    list_req.set_user_id(user1);
    nebula::proto::ListFriendsResponse list_resp;
    assert(service.ListFriends(&server_context, &list_req, &list_resp).ok());
    assert(list_resp.response().code() == 0);
    bool found = false;
    for (const auto& user : list_resp.friends()) if (user.user_id() == user2) found = true;
    assert(found);

    nebula::proto::CommonResponse repeat_add;
    assert(service.AddFriend(&server_context, &add, &repeat_add).ok());
    assert(repeat_add.code() == static_cast<int>(nebula::ErrorCode::FRIEND_REQUEST_REQUIRED));

    nebula::proto::DeleteFriendRequest del;
    del.set_request_id("del");
    del.set_user_id(user1);
    del.set_friend_id(user2);
    nebula::proto::CommonResponse del_resp;
    assert(service.DeleteFriend(&server_context, &del, &del_resp).ok());
    assert(del_resp.code() == 0);
    nebula::proto::CommonResponse del_again;
    assert(service.DeleteFriend(&server_context, &del, &del_again).ok());
    assert(del_again.code() == static_cast<int>(nebula::ErrorCode::FRIEND_NOT_FOUND));

    nebula::proto::CreateGroupRequest create_group;
    create_group.set_request_id("create-group");
    create_group.set_owner_id(user1);
    create_group.set_group_name("test_group");
    nebula::proto::CreateGroupResponse create_group_resp;
    assert(service.CreateGroup(&server_context, &create_group, &create_group_resp).ok());
    assert(create_group_resp.response().code() == 0);
    uint64_t group_id = create_group_resp.group_id();
    assert(group_id > 0);

    nebula::proto::JoinGroupRequest join;
    join.set_request_id("join");
    join.set_user_id(user2);
    join.set_group_id(group_id);
    nebula::proto::CommonResponse join_resp;
    assert(service.JoinGroup(&server_context, &join, &join_resp).ok());
    assert(join_resp.code() == 0);

    nebula::proto::ListGroupMembersRequest members_req;
    members_req.set_request_id("members");
    members_req.set_group_id(group_id);
    members_req.set_requester_user_id(user1);
    nebula::proto::ListGroupMembersResponse members_resp;
    assert(service.ListGroupMembers(&server_context, &members_req, &members_resp).ok());
    assert(members_resp.response().code() == 0);
    bool has_owner = false, has_member = false;
    for (const auto& user : members_resp.members()) {
        if (user.user_id() == user1) has_owner = true;
        if (user.user_id() == user2) has_member = true;
    }
    assert(has_owner && has_member);

    nebula::proto::ListGroupMembersRequest denied_members_req = members_req;
    denied_members_req.set_request_id("members-denied");
    denied_members_req.set_requester_user_id(user3);
    nebula::proto::ListGroupMembersResponse denied_members_resp;
    assert(service.ListGroupMembers(&server_context, &denied_members_req, &denied_members_resp).ok());
    assert(denied_members_resp.response().code() == static_cast<int>(nebula::ErrorCode::GROUP_NOT_MEMBER));
    assert(denied_members_resp.members().empty());

    nebula::proto::LeaveGroupRequest leave_member;
    leave_member.set_request_id("leave-member");
    leave_member.set_user_id(user2);
    leave_member.set_group_id(group_id);
    nebula::proto::CommonResponse leave_member_resp;
    assert(service.LeaveGroup(&server_context, &leave_member, &leave_member_resp).ok());
    assert(leave_member_resp.code() == 0);

    nebula::proto::LeaveGroupRequest leave_owner;
    leave_owner.set_request_id("leave-owner");
    leave_owner.set_user_id(user1);
    leave_owner.set_group_id(group_id);
    nebula::proto::CommonResponse leave_owner_resp;
    assert(service.LeaveGroup(&server_context, &leave_owner, &leave_owner_resp).ok());
    assert(leave_owner_resp.code() == static_cast<int>(nebula::ErrorCode::GROUP_OWNER_CANNOT_LEAVE));

    nebula::proto::JoinGroupRequest bad_join;
    bad_join.set_request_id("bad-join");
    bad_join.set_user_id(user3);
    bad_join.set_group_id(999999999);
    nebula::proto::CommonResponse bad_join_resp;
    assert(service.JoinGroup(&server_context, &bad_join, &bad_join_resp).ok());
    assert(bad_join_resp.code() == static_cast<int>(nebula::ErrorCode::GROUP_NOT_FOUND));

    nebula::proto::AddFriendRequest bad_add;
    bad_add.set_request_id("bad-add");
    bad_add.set_user_id(999999999);
    bad_add.set_friend_id(user3);
    nebula::proto::CommonResponse bad_add_resp;
    assert(service.AddFriend(&server_context, &bad_add, &bad_add_resp).ok());
    assert(bad_add_resp.code() == static_cast<int>(nebula::ErrorCode::FRIEND_REQUEST_REQUIRED));

    return 0;
}
