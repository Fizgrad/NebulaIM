#include "RelationServiceImpl.h"

#include "common/dao/FriendRequestDao.h"
#include "common/dao/GroupDao.h"
#include "common/dao/RelationDao.h"
#include "common/dao/UserDao.h"
#include "common/error/ErrorCode.h"
#include "common/log/Logger.h"
#include "common/rpc/InternalRpcAuth.h"
#include "common/utils/TimeUtil.h"

#include <algorithm>
#include <atomic>

namespace nebula {

namespace {

void fillResponse(proto::CommonResponse* response, const std::string& request_id, ErrorCode code, const std::string& message = "") {
    response->set_code(static_cast<int32_t>(code));
    response->set_message(message.empty() ? errorCodeToString(code) : message);
    response->set_request_id(request_id.empty() ? "request-id-empty" : request_id);
}

bool requireInternalRpc(grpc::ServerContext* context, const std::string& request_id, proto::CommonResponse* response) {
    if (InternalRpcAuth::instance().authorize(context)) return true;
    fillResponse(response, request_id, ErrorCode::AUTH_FAILED, "internal rpc unauthenticated");
    return false;
}

void fillUser(proto::UserInfo* info, const User& user) {
    info->set_user_id(user.id);
    info->set_username(user.username);
    info->set_nickname(user.nickname);
    info->set_avatar(user.avatar);
    info->set_created_at(user.created_at);
}

void fillFriendRequest(proto::FriendRequestInfo* info, const FriendRequest& request) {
    info->set_friend_request_id(request.request_id);
    info->set_from_user_id(request.from_user_id);
    info->set_to_user_id(request.to_user_id);
    info->set_message(request.message);
    info->set_status(request.status);
    info->set_created_at(request.created_at);
    info->set_updated_at(request.updated_at);
}

bool invalidDeps(UserDao* user_dao, RelationDao* relation_dao, GroupDao* group_dao) {
    return user_dao == nullptr || relation_dao == nullptr || group_dao == nullptr;
}

}  // namespace

RelationServiceImpl::RelationServiceImpl(UserDao* user_dao, RelationDao* relation_dao, GroupDao* group_dao, FriendRequestDao* friend_request_dao)
    : user_dao_(user_dao), relation_dao_(relation_dao), group_dao_(group_dao), friend_request_dao_(friend_request_dao) {}

grpc::Status RelationServiceImpl::AddFriend(grpc::ServerContext* context, const proto::AddFriendRequest* request, proto::CommonResponse* response) {
    LOG_INFO("AddFriend request_id=" + request->request_id() + " user_id=" + std::to_string(request->user_id()) + " friend_id=" + std::to_string(request->friend_id()));
    if (!requireInternalRpc(context, request->request_id(), response)) return grpc::Status::OK;
    if (request->user_id() == 0 || request->friend_id() == 0) { fillResponse(response, request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    if (request->user_id() == request->friend_id()) { fillResponse(response, request->request_id(), ErrorCode::CANNOT_ADD_SELF); return grpc::Status::OK; }
    fillResponse(response, request->request_id(), ErrorCode::FRIEND_REQUEST_REQUIRED, "use SendFriendRequest and AcceptFriendRequest");
    return grpc::Status::OK;
}

grpc::Status RelationServiceImpl::DeleteFriend(grpc::ServerContext* context, const proto::DeleteFriendRequest* request, proto::CommonResponse* response) {
    LOG_INFO("DeleteFriend request_id=" + request->request_id() + " user_id=" + std::to_string(request->user_id()) + " friend_id=" + std::to_string(request->friend_id()));
    if (!requireInternalRpc(context, request->request_id(), response)) return grpc::Status::OK;
    if (invalidDeps(user_dao_, relation_dao_, group_dao_)) { fillResponse(response, request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->user_id() == 0 || request->friend_id() == 0 || request->user_id() == request->friend_id()) { fillResponse(response, request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    if (!relation_dao_->isFriend(request->user_id(), request->friend_id())) { fillResponse(response, request->request_id(), ErrorCode::FRIEND_NOT_FOUND); return grpc::Status::OK; }
    if (!relation_dao_->deleteFriendBidirectional(request->user_id(), request->friend_id())) { fillResponse(response, request->request_id(), ErrorCode::DB_ERROR); return grpc::Status::OK; }
    fillResponse(response, request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status RelationServiceImpl::ListFriends(grpc::ServerContext* context, const proto::ListFriendsRequest* request, proto::ListFriendsResponse* response) {
    LOG_INFO("ListFriends request_id=" + request->request_id() + " user_id=" + std::to_string(request->user_id()));
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
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

grpc::Status RelationServiceImpl::SendFriendRequest(grpc::ServerContext* context, const proto::SendFriendRequestRequest* request, proto::SendFriendRequestResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
    if (invalidDeps(user_dao_, relation_dao_, group_dao_) || friend_request_dao_ == nullptr) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->from_user_id() == 0 || request->to_user_id() == 0 || request->from_user_id() == request->to_user_id()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    if (!user_dao_->getUserById(request->from_user_id()).has_value() || !user_dao_->getUserById(request->to_user_id()).has_value()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::USER_NOT_FOUND); return grpc::Status::OK; }
    if (relation_dao_->isFriend(request->from_user_id(), request->to_user_id())) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::FRIEND_ALREADY_EXISTS); return grpc::Status::OK; }

    static std::atomic<uint64_t> seq{1};
    int64_t now = TimeUtil::nowMs();
    FriendRequest item;
    item.request_id = static_cast<uint64_t>(now) * 1000 + (seq.fetch_add(1, std::memory_order_relaxed) % 1000);
    item.from_user_id = request->from_user_id();
    item.to_user_id = request->to_user_id();
    item.message = request->message().substr(0, 255);
    item.status = static_cast<int>(FriendRequestStatus::PENDING);
    item.created_at = now;
    item.updated_at = now;
    if (!friend_request_dao_->createRequest(item)) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::FRIEND_REQUEST_ALREADY_EXISTS); return grpc::Status::OK; }
    response->set_friend_request_id(item.request_id);
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status RelationServiceImpl::AcceptFriendRequest(grpc::ServerContext* context, const proto::AcceptFriendRequestRequest* request, proto::CommonResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response)) return grpc::Status::OK;
    if (invalidDeps(user_dao_, relation_dao_, group_dao_) || friend_request_dao_ == nullptr) { fillResponse(response, request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    auto item = friend_request_dao_->getByRequestId(request->friend_request_id());
    if (!item.has_value()) { fillResponse(response, request->request_id(), ErrorCode::FRIEND_REQUEST_NOT_FOUND); return grpc::Status::OK; }
    if (item->to_user_id != request->user_id()) { fillResponse(response, request->request_id(), ErrorCode::GATEWAY_PERMISSION_DENIED); return grpc::Status::OK; }
    if (item->status != static_cast<int>(FriendRequestStatus::PENDING)) { fillResponse(response, request->request_id(), ErrorCode::FRIEND_REQUEST_ALREADY_HANDLED); return grpc::Status::OK; }
    if (!friend_request_dao_->acceptRequestWithFriendship(request->friend_request_id(), *relation_dao_)) { fillResponse(response, request->request_id(), ErrorCode::DB_ERROR); return grpc::Status::OK; }
    fillResponse(response, request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status RelationServiceImpl::RejectFriendRequest(grpc::ServerContext* context, const proto::RejectFriendRequestRequest* request, proto::CommonResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response)) return grpc::Status::OK;
    if (friend_request_dao_ == nullptr) { fillResponse(response, request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    auto item = friend_request_dao_->getByRequestId(request->friend_request_id());
    if (!item.has_value()) { fillResponse(response, request->request_id(), ErrorCode::FRIEND_REQUEST_NOT_FOUND); return grpc::Status::OK; }
    if (item->to_user_id != request->user_id()) { fillResponse(response, request->request_id(), ErrorCode::GATEWAY_PERMISSION_DENIED); return grpc::Status::OK; }
    if (item->status != static_cast<int>(FriendRequestStatus::PENDING)) { fillResponse(response, request->request_id(), ErrorCode::FRIEND_REQUEST_ALREADY_HANDLED); return grpc::Status::OK; }
    if (!friend_request_dao_->updateStatus(request->friend_request_id(), FriendRequestStatus::PENDING, FriendRequestStatus::REJECTED)) { fillResponse(response, request->request_id(), ErrorCode::DB_ERROR); return grpc::Status::OK; }
    fillResponse(response, request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status RelationServiceImpl::ListFriendRequests(grpc::ServerContext* context, const proto::ListFriendRequestsRequest* request, proto::ListFriendRequestsResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
    if (friend_request_dao_ == nullptr) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->user_id() == 0) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    size_t limit = request->page().page_size() == 0 ? 50 : request->page().page_size();
    if (limit > 200) limit = 200;
    auto items = request->incoming() ? friend_request_dao_->listReceived(request->user_id(), request->status(), limit)
                                     : friend_request_dao_->listSent(request->user_id(), request->status(), limit);
    for (const auto& item : items) fillFriendRequest(response->add_requests(), item);
    response->mutable_page()->set_page(request->page().page());
    response->mutable_page()->set_page_size(static_cast<uint32_t>(limit));
    response->mutable_page()->set_total(response->requests_size());
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status RelationServiceImpl::CreateGroup(grpc::ServerContext* context, const proto::CreateGroupRequest* request, proto::CreateGroupResponse* response) {
    LOG_INFO("CreateGroup request_id=" + request->request_id() + " owner_id=" + std::to_string(request->owner_id()));
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
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

grpc::Status RelationServiceImpl::JoinGroup(grpc::ServerContext* context, const proto::JoinGroupRequest* request, proto::CommonResponse* response) {
    LOG_INFO("JoinGroup request_id=" + request->request_id() + " user_id=" + std::to_string(request->user_id()) + " group_id=" + std::to_string(request->group_id()));
    if (!requireInternalRpc(context, request->request_id(), response)) return grpc::Status::OK;
    if (invalidDeps(user_dao_, relation_dao_, group_dao_)) { fillResponse(response, request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->user_id() == 0 || request->group_id() == 0) { fillResponse(response, request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    if (!user_dao_->getUserById(request->user_id()).has_value()) { fillResponse(response, request->request_id(), ErrorCode::USER_NOT_FOUND); return grpc::Status::OK; }
    if (!group_dao_->getGroupById(request->group_id()).has_value()) { fillResponse(response, request->request_id(), ErrorCode::GROUP_NOT_FOUND); return grpc::Status::OK; }
    if (group_dao_->isMember(request->group_id(), request->user_id())) { fillResponse(response, request->request_id(), ErrorCode::GROUP_ALREADY_JOINED); return grpc::Status::OK; }
    if (!group_dao_->addMember(request->group_id(), request->user_id(), 0)) { fillResponse(response, request->request_id(), ErrorCode::DB_ERROR); return grpc::Status::OK; }
    fillResponse(response, request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status RelationServiceImpl::LeaveGroup(grpc::ServerContext* context, const proto::LeaveGroupRequest* request, proto::CommonResponse* response) {
    LOG_INFO("LeaveGroup request_id=" + request->request_id() + " user_id=" + std::to_string(request->user_id()) + " group_id=" + std::to_string(request->group_id()));
    if (!requireInternalRpc(context, request->request_id(), response)) return grpc::Status::OK;
    if (invalidDeps(user_dao_, relation_dao_, group_dao_)) { fillResponse(response, request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->user_id() == 0 || request->group_id() == 0) { fillResponse(response, request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    if (!group_dao_->getGroupById(request->group_id()).has_value()) { fillResponse(response, request->request_id(), ErrorCode::GROUP_NOT_FOUND); return grpc::Status::OK; }
    if (!group_dao_->isMember(request->group_id(), request->user_id())) { fillResponse(response, request->request_id(), ErrorCode::GROUP_NOT_MEMBER); return grpc::Status::OK; }
    if (group_dao_->isOwner(request->group_id(), request->user_id())) { fillResponse(response, request->request_id(), ErrorCode::GROUP_OWNER_CANNOT_LEAVE); return grpc::Status::OK; }
    if (!group_dao_->removeMember(request->group_id(), request->user_id())) { fillResponse(response, request->request_id(), ErrorCode::DB_ERROR); return grpc::Status::OK; }
    fillResponse(response, request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status RelationServiceImpl::ListGroupMembers(grpc::ServerContext* context, const proto::ListGroupMembersRequest* request, proto::ListGroupMembersResponse* response) {
    LOG_INFO("ListGroupMembers request_id=" + request->request_id() + " group_id=" + std::to_string(request->group_id()));
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
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
