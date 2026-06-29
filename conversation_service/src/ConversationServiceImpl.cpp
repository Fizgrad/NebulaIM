#include "ConversationServiceImpl.h"

#include "common/conversation/ConversationDao.h"
#include "common/error/ErrorCode.h"

namespace nebula {

namespace {

void fillResponse(proto::CommonResponse* response, const std::string& request_id, ErrorCode code, const std::string& message = "") {
    response->set_code(static_cast<int32_t>(code));
    response->set_message(message.empty() ? errorCodeToString(code) : message);
    response->set_request_id(request_id.empty() ? "request-id-empty" : request_id);
}

void fillConversation(proto::ConversationInfo* info, const Conversation& conv) {
    info->set_conversation_id(conv.conversation_id);
    info->set_conversation_type(conv.conversation_type);
    info->set_owner_user_id(conv.owner_user_id);
    info->set_peer_user_id(conv.peer_user_id);
    info->set_group_id(conv.group_id);
    info->set_last_message_id(conv.last_message_id);
    info->set_last_message_preview(conv.last_message_preview);
    info->set_last_message_at(conv.last_message_at);
    info->set_unread_count(conv.unread_count);
    info->set_pinned(conv.pinned);
    info->set_muted(conv.muted);
    info->set_deleted(conv.deleted);
    info->set_updated_at(conv.updated_at);
}

}  // namespace

ConversationServiceImpl::ConversationServiceImpl(ConversationDao* conversation_dao) : conversation_dao_(conversation_dao) {}

grpc::Status ConversationServiceImpl::ListConversations(grpc::ServerContext*, const proto::ListConversationsRequest* request, proto::ListConversationsResponse* response) {
    if (conversation_dao_ == nullptr) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->user_id() == 0) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    size_t page_size = request->page().page_size() == 0 ? 50 : request->page().page_size();
    if (page_size > 200) page_size = 200;
    size_t page = request->page().page() == 0 ? 1 : request->page().page();
    auto rows = conversation_dao_->listConversations(request->user_id(), page_size, (page - 1) * page_size);
    for (const auto& row : rows) fillConversation(response->add_conversations(), row);
    response->mutable_page()->set_page(static_cast<uint32_t>(page));
    response->mutable_page()->set_page_size(static_cast<uint32_t>(page_size));
    response->mutable_page()->set_total(response->conversations_size());
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status ConversationServiceImpl::MarkConversationRead(grpc::ServerContext*, const proto::ConversationMarkReadRequest* request, proto::CommonResponse* response) {
    if (conversation_dao_ == nullptr) { fillResponse(response, request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    bool ok = conversation_dao_->markRead(request->user_id(), request->conversation_id());
    fillResponse(response, request->request_id(), ok ? ErrorCode::OK : ErrorCode::CONVERSATION_NOT_FOUND, ok ? "OK" : "");
    return grpc::Status::OK;
}

grpc::Status ConversationServiceImpl::DeleteConversation(grpc::ServerContext*, const proto::ConversationDeleteRequest* request, proto::CommonResponse* response) {
    if (conversation_dao_ == nullptr) { fillResponse(response, request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    bool ok = conversation_dao_->deleteConversation(request->user_id(), request->conversation_id());
    fillResponse(response, request->request_id(), ok ? ErrorCode::OK : ErrorCode::CONVERSATION_NOT_FOUND, ok ? "OK" : "");
    return grpc::Status::OK;
}

grpc::Status ConversationServiceImpl::PinConversation(grpc::ServerContext*, const proto::ConversationPinRequest* request, proto::CommonResponse* response) {
    if (conversation_dao_ == nullptr) { fillResponse(response, request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    bool ok = conversation_dao_->pinConversation(request->user_id(), request->conversation_id(), request->pinned());
    fillResponse(response, request->request_id(), ok ? ErrorCode::OK : ErrorCode::CONVERSATION_NOT_FOUND, ok ? "OK" : "");
    return grpc::Status::OK;
}

grpc::Status ConversationServiceImpl::MuteConversation(grpc::ServerContext*, const proto::ConversationMuteRequest* request, proto::CommonResponse* response) {
    if (conversation_dao_ == nullptr) { fillResponse(response, request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    bool ok = conversation_dao_->muteConversation(request->user_id(), request->conversation_id(), request->muted());
    fillResponse(response, request->request_id(), ok ? ErrorCode::OK : ErrorCode::CONVERSATION_NOT_FOUND, ok ? "OK" : "");
    return grpc::Status::OK;
}

}  // namespace nebula
