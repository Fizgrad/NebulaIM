#include "RelationServiceImpl.h"

#include "common/dao/GroupDao.h"
#include "common/dao/RelationDao.h"
#include "common/dao/UserDao.h"
#include "common/error/ErrorCode.h"
#include "common/log/Logger.h"
#include "common/utils/TimeUtil.h"

#include <algorithm>

namespace nebula {

namespace {

void fillResponse(proto::CommonResponse* response, const std::string& request_id, ErrorCode code, const std::string& message = "") {
    response->set_code(static_cast<int32_t>(code));
    response->set_message(message.empty() ? errorCodeToString(code) : message);
    response->set_request_id(request_id.empty() ? "request-id-empty" : request_id);
}

void fillUser(proto::UserInfo* info, const User& user) {
    info->set_user_id(user.id);
    info->set_username(user.username);
    info->set_nickname(user.nickname);
    info->set_avatar(user.avatar);
    info->set_created_at(user.created_at);
}

bool invalidDeps(UserDao* user_dao, RelationDao* relation_dao, GroupDao* group_dao) {
    return user_dao == nullptr || relation_dao == nullptr || group_dao == nullptr;
}

}  // namespace

RelationServiceImpl::RelationServiceImpl(UserDao* user_dao, RelationDao* relation_dao, GroupDao* group_dao)
    : user_dao_(user_dao), relation_dao_(relation_dao), group_dao_(group_dao) {}

grpc::Status RelationServiceImpl::AddFriend(grpc::ServerContext*, const proto::AddFriendRequest* request, proto::CommonResponse* response) {
    LOG_INFO("AddFriend request_id=" + request->request_id() + " user_id=" + std::to_string(request->user_id()) + " friend_id=" + std::to_string(request->friend_id()));
    if (invalidDeps(user_dao_, relation_dao_, group_dao_)) { fillResponse(response, request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->user_id() == 0 || request->friend_id() == 0) { fillResponse(response, request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    if (request->user_id() == request->friend_id()) { fillResponse(response, request->request_id(), ErrorCode::CANNOT_ADD_SELF); return grpc::Status::OK; }
    if (!user_dao_->getUserById(request->user_id()).has_value() || !user_dao_->getUserById(request->friend_id()).has_value()) { fillResponse(response, request->request_id(), ErrorCode::USER_NOT_FOUND); return grpc::Status::OK; }
    if (relation_dao_->isFriend(request->user_id(), request->friend_id())) { fillResponse(response, request->request_id(), ErrorCode::FRIEND_ALREADY_EXISTS); return grpc::Status::OK; }
    if (!relation_dao_->addFriendBidirectional(request->user_id(), request->friend_id())) { fillResponse(response, request->request_id(), ErrorCode::DB_ERROR); return grpc::Status::OK; }
    fillResponse(response, request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status RelationServiceImpl::DeleteFriend(grpc::ServerContext*, const proto::DeleteFriendRequest* request, proto::CommonResponse* response) {
    LOG_INFO("DeleteFriend request_id=" + request->request_id() + " user_id=" + std::to_string(request->user_id()) + " friend_id=" + std::to_string(request->friend_id()));
    if (invalidDeps(user_dao_, relation_dao_, group_dao_)) { fillResponse(response, request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->user_id() == 0 || request->friend_id() == 0 || request->user_id() == request->friend_id()) { fillResponse(response, request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    if (!relation_dao_->isFriend(request->user_id(), request->friend_id())) { fillResponse(response, request->request_id(), ErrorCode::FRIEND_NOT_FOUND); return grpc::Status::OK; }
    if (!relation_dao_->deleteFriendBidirectional(request->user_id(), request->friend_id())) { fillResponse(response, request->request_id(), ErrorCode::DB_ERROR); return grpc::Status::OK; }
    fillResponse(response, request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status RelationServiceImpl::ListFriends(grpc::ServerContext*, const proto::ListFriendsRequest* request, proto::ListFriendsResponse* response) {
    LOG_INFO("ListFriends request_id=" + request->request_id() + " user_id=" + std::to_string(request->user_id()));
    if (invalidDeps(user_dao_, relation_dao_, group_dao_)) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->user_id() == 0) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    if (!user_dao_->getUserById(request->user_id()).has_value()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::USER_NOT_FOUND); return grpc::Status::OK; }
    for (uint64_t friend_id : relation_dao_->listFriends(request->user_id())) {
        auto user = user_dao_->getUserById(friend_id);
        if (user.has_value()) fillUser(response->add_friends(), user.value());
    }
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status RelationServiceImpl::CreateGroup(grpc::ServerContext*, const proto::CreateGroupRequest* request, proto::CreateGroupResponse* response) {
    LOG_INFO("CreateGroup request_id=" + request->request_id() + " owner_id=" + std::to_string(request->owner_id()));
    if (invalidDeps(user_dao_, relation_dao_, group_dao_)) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->owner_id() == 0 || request->group_name().empty() || request->group_name().size() > 128) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    if (!user_dao_->getUserById(request->owner_id()).has_value()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::USER_NOT_FOUND); return grpc::Status::OK; }
    Group group;
    group.group_name = request->group_name();
    group.owner_id = request->owner_id();
    group.created_at = TimeUtil::nowMs();
    group.updated_at = group.created_at;
    uint64_t group_id = 0;
    if (!group_dao_->createGroup(group, &group_id)) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::DB_ERROR); return grpc::Status::OK; }
    response->set_group_id(group_id);
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status RelationServiceImpl::JoinGroup(grpc::ServerContext*, const proto::JoinGroupRequest* request, proto::CommonResponse* response) {
    LOG_INFO("JoinGroup request_id=" + request->request_id() + " user_id=" + std::to_string(request->user_id()) + " group_id=" + std::to_string(request->group_id()));
    if (invalidDeps(user_dao_, relation_dao_, group_dao_)) { fillResponse(response, request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->user_id() == 0 || request->group_id() == 0) { fillResponse(response, request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    if (!user_dao_->getUserById(request->user_id()).has_value()) { fillResponse(response, request->request_id(), ErrorCode::USER_NOT_FOUND); return grpc::Status::OK; }
    if (!group_dao_->getGroupById(request->group_id()).has_value()) { fillResponse(response, request->request_id(), ErrorCode::GROUP_NOT_FOUND); return grpc::Status::OK; }
    if (group_dao_->isMember(request->group_id(), request->user_id())) { fillResponse(response, request->request_id(), ErrorCode::GROUP_ALREADY_JOINED); return grpc::Status::OK; }
    if (!group_dao_->addMember(request->group_id(), request->user_id(), 0)) { fillResponse(response, request->request_id(), ErrorCode::DB_ERROR); return grpc::Status::OK; }
    fillResponse(response, request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status RelationServiceImpl::LeaveGroup(grpc::ServerContext*, const proto::LeaveGroupRequest* request, proto::CommonResponse* response) {
    LOG_INFO("LeaveGroup request_id=" + request->request_id() + " user_id=" + std::to_string(request->user_id()) + " group_id=" + std::to_string(request->group_id()));
    if (invalidDeps(user_dao_, relation_dao_, group_dao_)) { fillResponse(response, request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->user_id() == 0 || request->group_id() == 0) { fillResponse(response, request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    if (!group_dao_->getGroupById(request->group_id()).has_value()) { fillResponse(response, request->request_id(), ErrorCode::GROUP_NOT_FOUND); return grpc::Status::OK; }
    if (!group_dao_->isMember(request->group_id(), request->user_id())) { fillResponse(response, request->request_id(), ErrorCode::GROUP_NOT_MEMBER); return grpc::Status::OK; }
    if (group_dao_->isOwner(request->group_id(), request->user_id())) { fillResponse(response, request->request_id(), ErrorCode::GROUP_OWNER_CANNOT_LEAVE); return grpc::Status::OK; }
    if (!group_dao_->removeMember(request->group_id(), request->user_id())) { fillResponse(response, request->request_id(), ErrorCode::DB_ERROR); return grpc::Status::OK; }
    fillResponse(response, request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status RelationServiceImpl::ListGroupMembers(grpc::ServerContext*, const proto::ListGroupMembersRequest* request, proto::ListGroupMembersResponse* response) {
    LOG_INFO("ListGroupMembers request_id=" + request->request_id() + " group_id=" + std::to_string(request->group_id()));
    if (invalidDeps(user_dao_, relation_dao_, group_dao_)) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->group_id() == 0) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    if (!group_dao_->getGroupById(request->group_id()).has_value()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::GROUP_NOT_FOUND); return grpc::Status::OK; }
    for (uint64_t user_id : group_dao_->listMembers(request->group_id())) {
        auto user = user_dao_->getUserById(user_id);
        if (user.has_value()) fillUser(response->add_members(), user.value());
    }
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

}  // namespace nebula
