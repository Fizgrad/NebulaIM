#pragma once

#include "relation.grpc.pb.h"

namespace nebula {

class GroupDao;
class RelationDao;
class UserDao;

class RelationServiceImpl final : public proto::RelationService::Service {
public:
    RelationServiceImpl(UserDao* user_dao, RelationDao* relation_dao, GroupDao* group_dao);

    grpc::Status AddFriend(grpc::ServerContext* context,
                           const proto::AddFriendRequest* request,
                           proto::CommonResponse* response) override;
    grpc::Status DeleteFriend(grpc::ServerContext* context,
                              const proto::DeleteFriendRequest* request,
                              proto::CommonResponse* response) override;
    grpc::Status ListFriends(grpc::ServerContext* context,
                             const proto::ListFriendsRequest* request,
                             proto::ListFriendsResponse* response) override;
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

private:
    UserDao* user_dao_;
    RelationDao* relation_dao_;
    GroupDao* group_dao_;
};

}  // namespace nebula
