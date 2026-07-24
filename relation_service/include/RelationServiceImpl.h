#pragma once

#include "relation.grpc.pb.h"

namespace nebula {

class GroupDao;
class FriendRequestDao;
class RelationDao;
class UserDao;

class RelationServiceImpl final : public proto::RelationService::Service {
public:
    RelationServiceImpl(UserDao* user_dao, RelationDao* relation_dao, GroupDao* group_dao, FriendRequestDao* friend_request_dao = nullptr);

    grpc::Status DeleteFriend(grpc::ServerContext* context,
                              const proto::DeleteFriendRequest* request,
                              proto::CommonResponse* response) override;
    grpc::Status ListFriends(grpc::ServerContext* context,
                             const proto::ListFriendsRequest* request,
                             proto::ListFriendsResponse* response) override;
    grpc::Status SendFriendRequest(grpc::ServerContext* context,
                                   const proto::SendFriendRequestRequest* request,
                                   proto::SendFriendRequestResponse* response) override;
    grpc::Status AcceptFriendRequest(grpc::ServerContext* context,
                                     const proto::AcceptFriendRequestRequest* request,
                                     proto::CommonResponse* response) override;
    grpc::Status RejectFriendRequest(grpc::ServerContext* context,
                                     const proto::RejectFriendRequestRequest* request,
                                     proto::CommonResponse* response) override;
    grpc::Status ListFriendRequests(grpc::ServerContext* context,
                                    const proto::ListFriendRequestsRequest* request,
                                    proto::ListFriendRequestsResponse* response) override;
    grpc::Status CreateGroup(grpc::ServerContext* context,
                             const proto::CreateGroupRequest* request,
                             proto::CreateGroupResponse* response) override;
    grpc::Status JoinGroup(grpc::ServerContext* context,
                           const proto::JoinGroupRequest* request,
                           proto::CommonResponse* response) override;
    grpc::Status LeaveGroup(grpc::ServerContext* context,
                            const proto::LeaveGroupRequest* request,
                            proto::CommonResponse* response) override;
    grpc::Status ListGroupMembers(grpc::ServerContext* context,
                                  const proto::ListGroupMembersRequest* request,
                                  proto::ListGroupMembersResponse* response) override;
    grpc::Status GetGroup(grpc::ServerContext* context,
                          const proto::GetGroupRequest* request,
                          proto::GetGroupResponse* response) override;
    grpc::Status ListGroups(grpc::ServerContext* context,
                            const proto::ListGroupsRequest* request,
                            proto::ListGroupsResponse* response) override;
    grpc::Status SearchGroups(grpc::ServerContext* context,
                              const proto::SearchGroupsRequest* request,
                              proto::ListGroupsResponse* response) override;

private:
    UserDao* user_dao_;
    RelationDao* relation_dao_;
    GroupDao* group_dao_;
    FriendRequestDao* friend_request_dao_;
};

}  // namespace nebula
