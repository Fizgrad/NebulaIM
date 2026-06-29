#include "MessageServiceImpl.h"

#include "common/dao/GroupDao.h"
#include "common/dao/MessageDao.h"
#include "common/dao/OfflineMessageDao.h"
#include "common/dao/UserDao.h"
#include "common/error/ErrorCode.h"
#include "common/kafka/KafkaProducer.h"
#include "common/log/Logger.h"
#include "common/message/ConversationId.h"
#include "common/message/MessageDeduplicator.h"
#include "common/message/MessageIdGenerator.h"
#include "common/message/MessageKafkaPayload.h"
#include "common/redis/RedisClient.h"
#include "common/utils/TimeUtil.h"

namespace nebula {

namespace {
void fillResponse(proto::CommonResponse* response, const std::string& request_id, ErrorCode code, const std::string& message = "") {
    response->set_code(static_cast<int32_t>(code));
    response->set_message(message.empty() ? errorCodeToString(code) : message);
    response->set_request_id(request_id.empty() ? "request-id-empty" : request_id);
}

proto::MessageData toMessageData(const MessageRecord& record) {
    proto::MessageData data;
    data.set_message_id(record.message_id);
    data.set_conversation_id(record.conversation_id);
    data.set_from_user_id(record.from_user_id);
    data.set_to_user_id(record.to_user_id);
    data.set_group_id(record.group_id);
    data.set_content_type(proto::MESSAGE_CONTENT_TYPE_TEXT);
    data.set_content(record.content);
    data.set_status(static_cast<proto::MessageStatus>(record.status));
    data.set_timestamp(record.created_at);
    return data;
}
}

MessageServiceImpl::MessageServiceImpl(UserDao* user_dao,
                                       GroupDao* group_dao,
                                       MessageDao* message_dao,
                                       OfflineMessageDao* offline_message_dao,
                                       RedisClient* redis_client,
                                       KafkaProducer* kafka_producer,
                                       MessageIdGenerator* message_id_generator,
                                       MessageDeduplicator* message_deduplicator,
                                       MessageServiceOptions options)
    : user_dao_(user_dao), group_dao_(group_dao), message_dao_(message_dao), offline_message_dao_(offline_message_dao),
      redis_client_(redis_client), kafka_producer_(kafka_producer), message_id_generator_(message_id_generator),
      message_deduplicator_(message_deduplicator), options_(std::move(options)) {}

bool MessageServiceImpl::invalidDeps() const {
    return user_dao_ == nullptr || group_dao_ == nullptr || message_dao_ == nullptr || offline_message_dao_ == nullptr ||
           redis_client_ == nullptr || kafka_producer_ == nullptr || message_id_generator_ == nullptr || message_deduplicator_ == nullptr;
}

bool MessageServiceImpl::validateContent(const std::string& request_id, const std::string& content, proto::CommonResponse* response) const {
    if (content.empty()) {
        fillResponse(response, request_id, ErrorCode::MESSAGE_EMPTY);
        return false;
    }
    if (static_cast<int>(content.size()) > options_.max_content_length) {
        fillResponse(response, request_id, ErrorCode::MESSAGE_TOO_LARGE);
        return false;
    }
    return true;
}

grpc::Status MessageServiceImpl::SendSingleMessage(grpc::ServerContext*, const proto::SendSingleMessageRequest* request, proto::SendSingleMessageResponse* response) {
    LOG_INFO("SendSingleMessage request_id=" + request->request_id() + " from=" + std::to_string(request->from_user_id()) + " to=" + std::to_string(request->to_user_id()));
    if (invalidDeps()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->from_user_id() == 0 || request->to_user_id() == 0 || request->from_user_id() == request->to_user_id()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    if (!validateContent(request->request_id(), request->content(), response->mutable_response())) return grpc::Status::OK;
    if (!user_dao_->getUserById(request->from_user_id()).has_value() || !user_dao_->getUserById(request->to_user_id()).has_value()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::USER_NOT_FOUND); return grpc::Status::OK; }
    if (request->client_sequence_id() != 0) {
        auto existing = message_deduplicator_->getExistingMessageId(request->from_user_id(), request->client_sequence_id());
        if (existing.has_value()) {
            fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "DUPLICATED_REQUEST_REPLAYED");
            response->set_message_id(existing.value());
            response->set_server_timestamp(TimeUtil::nowMs());
            return grpc::Status::OK;
        }
    }
    uint64_t message_id = message_id_generator_->nextId();
    uint64_t conversation_id = ConversationId::single(request->from_user_id(), request->to_user_id());
    int64_t now = TimeUtil::nowMs();
    MessageRecord record;
    record.message_id = message_id;
    record.conversation_id = conversation_id;
    record.from_user_id = request->from_user_id();
    record.to_user_id = request->to_user_id();
    record.message_type = static_cast<int>(request->content_type());
    record.content = request->content();
    record.status = proto::MESSAGE_STATUS_SENT;
    record.created_at = now;
    record.updated_at = now;
    if (!message_dao_->insertMessage(record)) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::MESSAGE_PERSIST_FAILED); return grpc::Status::OK; }
    std::string payload = MessageKafkaPayload::serializeMessageData(toMessageData(record));
    if (!kafka_producer_->produce(options_.topic_single, std::to_string(conversation_id), payload)) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::MESSAGE_KAFKA_FAILED); return grpc::Status::OK; }
    kafka_producer_->flush(5000);
    if (request->client_sequence_id() != 0 && !message_deduplicator_->markMessage(request->from_user_id(), request->client_sequence_id(), message_id)) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::REDIS_ERROR); return grpc::Status::OK; }
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    response->set_message_id(message_id);
    response->set_server_timestamp(now);
    return grpc::Status::OK;
}

grpc::Status MessageServiceImpl::SendGroupMessage(grpc::ServerContext*, const proto::SendGroupMessageRequest* request, proto::SendGroupMessageResponse* response) {
    LOG_INFO("SendGroupMessage request_id=" + request->request_id() + " from=" + std::to_string(request->from_user_id()) + " group=" + std::to_string(request->group_id()));
    if (invalidDeps()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->from_user_id() == 0 || request->group_id() == 0) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    if (!validateContent(request->request_id(), request->content(), response->mutable_response())) return grpc::Status::OK;
    if (!user_dao_->getUserById(request->from_user_id()).has_value()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::USER_NOT_FOUND); return grpc::Status::OK; }
    if (!group_dao_->getGroupById(request->group_id()).has_value()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::GROUP_NOT_FOUND); return grpc::Status::OK; }
    if (!group_dao_->isMember(request->group_id(), request->from_user_id())) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::MESSAGE_PERMISSION_DENIED); return grpc::Status::OK; }
    if (request->client_sequence_id() != 0) {
        auto existing = message_deduplicator_->getExistingMessageId(request->from_user_id(), request->client_sequence_id());
        if (existing.has_value()) {
            fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "DUPLICATED_REQUEST_REPLAYED");
            response->set_message_id(existing.value());
            response->set_server_timestamp(TimeUtil::nowMs());
            return grpc::Status::OK;
        }
    }
    uint64_t message_id = message_id_generator_->nextId();
    uint64_t conversation_id = ConversationId::group(request->group_id());
    int64_t now = TimeUtil::nowMs();
    MessageRecord record;
    record.message_id = message_id;
    record.conversation_id = conversation_id;
    record.from_user_id = request->from_user_id();
    record.group_id = request->group_id();
    record.message_type = static_cast<int>(request->content_type());
    record.content = request->content();
    record.status = proto::MESSAGE_STATUS_SENT;
    record.created_at = now;
    record.updated_at = now;
    if (!message_dao_->insertMessage(record)) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::MESSAGE_PERSIST_FAILED); return grpc::Status::OK; }
    std::string payload = MessageKafkaPayload::serializeMessageData(toMessageData(record));
    if (!kafka_producer_->produce(options_.topic_group, std::to_string(conversation_id), payload)) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::MESSAGE_KAFKA_FAILED); return grpc::Status::OK; }
    kafka_producer_->flush(5000);
    if (request->client_sequence_id() != 0 && !message_deduplicator_->markMessage(request->from_user_id(), request->client_sequence_id(), message_id)) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::REDIS_ERROR); return grpc::Status::OK; }
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    response->set_message_id(message_id);
    response->set_server_timestamp(now);
    return grpc::Status::OK;
}

grpc::Status MessageServiceImpl::AckMessage(grpc::ServerContext*, const proto::AckMessageRequest* request, proto::AckMessageResponse* response) {
    if (invalidDeps()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->user_id() == 0 || request->message_id() == 0) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    if (!user_dao_->getUserById(request->user_id()).has_value()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::USER_NOT_FOUND); return grpc::Status::OK; }
    if (!message_dao_->messageExists(request->message_id())) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::MESSAGE_NOT_FOUND); return grpc::Status::OK; }
    message_dao_->updateMessageStatus(request->message_id(), proto::MESSAGE_STATUS_ACKED);
    offline_message_dao_->markAsDelivered(request->user_id(), request->message_id());
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status MessageServiceImpl::PullOfflineMessages(grpc::ServerContext*, const proto::PullOfflineMessagesRequest* request, proto::PullOfflineMessagesResponse* response) {
    if (invalidDeps()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->user_id() == 0) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    if (!user_dao_->getUserById(request->user_id()).has_value()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::USER_NOT_FOUND); return grpc::Status::OK; }
    size_t limit = request->page().page_size() == 0 ? static_cast<size_t>(options_.offline_pull_limit) : request->page().page_size();
    if (limit > static_cast<size_t>(options_.offline_pull_limit)) limit = static_cast<size_t>(options_.offline_pull_limit);
    auto offline = offline_message_dao_->listOfflineMessages(request->user_id(), limit);
    std::vector<uint64_t> delivered;
    for (const auto& item : offline) {
        proto::MessageData data;
        if (MessageKafkaPayload::deserializeMessageData(item.payload, &data)) {
            *response->add_messages() = data;
            delivered.push_back(item.message_id);
        } else {
            LOG_ERROR("failed to parse offline message payload message_id=" + std::to_string(item.message_id));
        }
    }
    offline_message_dao_->markBatchAsDelivered(request->user_id(), delivered);
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    response->mutable_page()->set_page(request->page().page());
    response->mutable_page()->set_page_size(static_cast<uint32_t>(limit));
    response->mutable_page()->set_total(response->messages_size());
    return grpc::Status::OK;
}

}  // namespace nebula
