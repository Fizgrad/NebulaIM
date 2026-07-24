#include "MessageServiceImpl.h"

#include "common/conversation/ConversationDao.h"
#include "common/conversation/ConversationServiceHelper.h"
#include "common/dao/GroupDao.h"
#include "common/dao/MessageDao.h"
#include "common/dao/MessageReceiptDao.h"
#include "common/dao/OfflineMessageDao.h"
#include "common/dao/RelationDao.h"
#include "common/dao/UserDao.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/db/MySqlTransaction.h"
#include "common/error/ErrorCode.h"
#include "common/kafka/KafkaProducer.h"
#include "common/log/Logger.h"
#include "common/message/ConversationId.h"
#include "common/message/MessageDeduplicator.h"
#include "common/message/MessageIdGenerator.h"
#include "common/message/MessageKafkaPayload.h"
#include "common/monitor/MetricsRegistry.h"
#include "common/outbox/OutboxDao.h"
#include "common/redis/RedisClient.h"
#include "common/rpc/InternalRpcAuth.h"
#include "common/rpc/RpcMetadata.h"
#include "common/trace/TraceContext.h"
#include "common/trace/TraceSpan.h"
#include "common/utils/TimeUtil.h"

#include <algorithm>

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

proto::MessageData toMessageData(const MessageRecord& record) {
    proto::MessageData data;
    data.set_message_id(record.message_id);
    data.set_conversation_id(record.conversation_id);
    data.set_from_user_id(record.from_user_id);
    data.set_to_user_id(record.to_user_id);
    data.set_group_id(record.group_id);
    data.set_content_type(static_cast<proto::MessageContentType>(record.message_type));
    data.set_content(record.content);
    data.set_status(static_cast<proto::MessageStatus>(record.status));
    data.set_timestamp(record.created_at);
    data.set_recalled(record.recalled);
    data.set_recalled_at(record.recalled_at);
    data.set_trace_id(TraceContext::traceId());
    return data;
}

OutboxEvent makeOutboxEvent(uint64_t event_id,
                            const std::string& aggregate_type,
                            uint64_t aggregate_id,
                            const std::string& topic,
                            const std::string& event_key,
                            const std::string& payload,
                            int64_t now) {
    OutboxEvent event;
    event.event_id = event_id;
    event.aggregate_type = aggregate_type;
    event.aggregate_id = aggregate_id;
    event.topic = topic;
    event.event_key = event_key;
    event.payload = payload;
    event.status = static_cast<int>(OutboxStatus::PENDING);
    event.created_at = now;
    event.updated_at = now;
    event.trace_id = TraceContext::traceId();
    return event;
}
}

MessageServiceImpl::MessageServiceImpl(UserDao* user_dao,
                                       GroupDao* group_dao,
                                       RelationDao* relation_dao,
                                       MessageDao* message_dao,
                                       OfflineMessageDao* offline_message_dao,
                                       RedisClient* redis_client,
                                       MessageIdGenerator* message_id_generator,
                                       MessageDeduplicator* message_deduplicator,
                                       MessageServiceOptions options,
                                       MySqlConnectionPool* mysql_pool,
                                       ConversationDao* conversation_dao,
                                       MessageReceiptDao* message_receipt_dao,
                                       OutboxDao* outbox_dao)
    : user_dao_(user_dao), group_dao_(group_dao), relation_dao_(relation_dao), message_dao_(message_dao), offline_message_dao_(offline_message_dao),
      redis_client_(redis_client), message_id_generator_(message_id_generator),
      message_deduplicator_(message_deduplicator), mysql_pool_(mysql_pool), conversation_dao_(conversation_dao),
      message_receipt_dao_(message_receipt_dao), outbox_dao_(outbox_dao), options_(std::move(options)) {}

bool MessageServiceImpl::invalidDeps() const {
    return user_dao_ == nullptr || group_dao_ == nullptr || relation_dao_ == nullptr || message_dao_ == nullptr || offline_message_dao_ == nullptr ||
           redis_client_ == nullptr || message_id_generator_ == nullptr || message_deduplicator_ == nullptr ||
           mysql_pool_ == nullptr || conversation_dao_ == nullptr || message_receipt_dao_ == nullptr || outbox_dao_ == nullptr;
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

bool MessageServiceImpl::canReceiveMessage(uint64_t user_id, const MessageRecord& record) const {
    if (user_id == 0 || record.from_user_id == user_id) return false;
    if (record.group_id != 0) {
        return group_dao_ != nullptr && group_dao_->isMember(record.group_id, user_id);
    }
    return record.to_user_id == user_id;
}

bool MessageServiceImpl::canReadConversation(uint64_t user_id, uint64_t conversation_id) const {
    return user_id != 0 && conversation_id != 0 && conversation_dao_ != nullptr && conversation_dao_->isOwner(user_id, conversation_id);
}

grpc::Status MessageServiceImpl::SendSingleMessage(grpc::ServerContext* context, const proto::SendSingleMessageRequest* request, proto::SendSingleMessageResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
    std::string inbound_trace = RpcMetadata::extractTraceId(context);
    TraceContext::Scope trace(TraceContext::ensureTraceId(inbound_trace.empty() ? request->request_id() : inbound_trace));
    TraceSpan span("message.SendSingleMessage", TraceSpanKind::SERVER);
    span.setAttribute("request_id", request->request_id());
    span.setAttribute("from_user_id", std::to_string(request->from_user_id()));
    span.setAttribute("to_user_id", std::to_string(request->to_user_id()));
    LOG_INFO("SendSingleMessage request_id=" + request->request_id() + " from=" + std::to_string(request->from_user_id()) + " to=" + std::to_string(request->to_user_id()));
    MetricsRegistry::instance().counter("nebula_message_send_single_total", "MessageService single-message send requests").inc();
    if (invalidDeps()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->from_user_id() == 0 || request->to_user_id() == 0 || request->from_user_id() == request->to_user_id()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    if (!validateContent(request->request_id(), request->content(), response->mutable_response())) return grpc::Status::OK;
    if (!user_dao_->getUserById(request->from_user_id()).has_value() || !user_dao_->getUserById(request->to_user_id()).has_value()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::USER_NOT_FOUND); return grpc::Status::OK; }
    if (!relation_dao_->isFriend(request->from_user_id(), request->to_user_id()) || !relation_dao_->isFriend(request->to_user_id(), request->from_user_id())) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::MESSAGE_PERMISSION_DENIED, "single messages require an accepted friendship");
        return grpc::Status::OK;
    }
    if (request->client_sequence_id() != 0) {
        auto existing = message_deduplicator_->getExistingMessageId(request->from_user_id(), request->client_sequence_id());
        std::optional<MessageRecord> persisted;
        if (existing.has_value()) {
            persisted = message_dao_->getMessageById(existing.value());
            if (!persisted.has_value() || persisted->from_user_id != request->from_user_id() ||
                persisted->client_sequence_id != request->client_sequence_id()) {
                persisted.reset();
            }
        }
        if (!persisted.has_value()) {
            persisted = message_dao_->getMessageByClientSequence(request->from_user_id(), request->client_sequence_id());
        }
        if (persisted.has_value()) {
            fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "DUPLICATED_REQUEST_REPLAYED");
            response->set_message_id(persisted->message_id);
            response->set_server_timestamp(persisted->created_at);
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
    record.client_sequence_id = request->client_sequence_id();
    record.to_user_id = request->to_user_id();
    record.message_type = static_cast<int>(request->content_type());
    record.content = request->content();
    record.status = proto::MESSAGE_STATUS_SENT;
    record.created_at = now;
    record.updated_at = now;
    std::string payload = MessageKafkaPayload::serializeMessageData(toMessageData(record));

    {
        auto conn = mysql_pool_->acquire();
        if (!conn) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::DB_ERROR); return grpc::Status::OK; }
        MySqlTransaction tx(*conn);
        if (!tx.active()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::DB_ERROR); return grpc::Status::OK; }
        if (!message_dao_->insertMessage(*conn, record)) {
            tx.rollback();
            auto existing = message_dao_->getMessageByClientSequence(request->from_user_id(), request->client_sequence_id());
            if (existing.has_value()) {
                fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "DUPLICATED_REQUEST_REPLAYED");
                response->set_message_id(existing->message_id);
                response->set_server_timestamp(existing->created_at);
                return grpc::Status::OK;
            }
            fillResponse(response->mutable_response(), request->request_id(), ErrorCode::MESSAGE_PERSIST_FAILED);
            return grpc::Status::OK;
        }
        if (!conversation_dao_->upsertSingleConversationForUsers(*conn, conversation_id, request->from_user_id(), request->to_user_id(), message_id, ConversationServiceHelper::buildPreview(request->content()), now)) {
            fillResponse(response->mutable_response(), request->request_id(), ErrorCode::DB_ERROR);
            return grpc::Status::OK;
        }
        auto event = makeOutboxEvent(message_id, "message", message_id, options_.topic_single, std::to_string(conversation_id), payload, now);
        if (!outbox_dao_->insertEvent(*conn, event)) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OUTBOX_PUBLISH_FAILED); return grpc::Status::OK; }
        if (!tx.commit()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::DB_ERROR); return grpc::Status::OK; }
    }
    if (request->client_sequence_id() != 0 && !message_deduplicator_->markMessage(request->from_user_id(), request->client_sequence_id(), message_id)) {
        LOG_WARN("message dedup mark failed after single message commit request_id=" + request->request_id() +
                 " user_id=" + std::to_string(request->from_user_id()) +
                 " client_sequence_id=" + std::to_string(request->client_sequence_id()) +
                 " message_id=" + std::to_string(message_id));
    }
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    response->set_message_id(message_id);
    response->set_server_timestamp(now);
    MetricsRegistry::instance().counter("nebula_message_send_single_success_total", "MessageService successful single-message sends").inc();
    return grpc::Status::OK;
}

grpc::Status MessageServiceImpl::SendGroupMessage(grpc::ServerContext* context, const proto::SendGroupMessageRequest* request, proto::SendGroupMessageResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
    std::string inbound_trace = RpcMetadata::extractTraceId(context);
    TraceContext::Scope trace(TraceContext::ensureTraceId(inbound_trace.empty() ? request->request_id() : inbound_trace));
    TraceSpan span("message.SendGroupMessage", TraceSpanKind::SERVER);
    span.setAttribute("request_id", request->request_id());
    span.setAttribute("from_user_id", std::to_string(request->from_user_id()));
    span.setAttribute("group_id", std::to_string(request->group_id()));
    LOG_INFO("SendGroupMessage request_id=" + request->request_id() + " from=" + std::to_string(request->from_user_id()) + " group=" + std::to_string(request->group_id()));
    MetricsRegistry::instance().counter("nebula_message_send_group_total", "MessageService group-message send requests").inc();
    if (invalidDeps()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->from_user_id() == 0 || request->group_id() == 0) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    if (!validateContent(request->request_id(), request->content(), response->mutable_response())) return grpc::Status::OK;
    if (!user_dao_->getUserById(request->from_user_id()).has_value()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::USER_NOT_FOUND); return grpc::Status::OK; }
    if (!group_dao_->getGroupById(request->group_id()).has_value()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::GROUP_NOT_FOUND); return grpc::Status::OK; }
    if (!group_dao_->isMember(request->group_id(), request->from_user_id())) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::MESSAGE_PERMISSION_DENIED); return grpc::Status::OK; }
    auto members = group_dao_->listMembers(request->group_id());
    if (request->client_sequence_id() != 0) {
        auto existing = message_deduplicator_->getExistingMessageId(request->from_user_id(), request->client_sequence_id());
        std::optional<MessageRecord> persisted;
        if (existing.has_value()) {
            persisted = message_dao_->getMessageById(existing.value());
            if (!persisted.has_value() || persisted->from_user_id != request->from_user_id() ||
                persisted->client_sequence_id != request->client_sequence_id()) {
                persisted.reset();
            }
        }
        if (!persisted.has_value()) {
            persisted = message_dao_->getMessageByClientSequence(request->from_user_id(), request->client_sequence_id());
        }
        if (persisted.has_value()) {
            fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "DUPLICATED_REQUEST_REPLAYED");
            response->set_message_id(persisted->message_id);
            response->set_server_timestamp(persisted->created_at);
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
    record.client_sequence_id = request->client_sequence_id();
    record.group_id = request->group_id();
    record.message_type = static_cast<int>(request->content_type());
    record.content = request->content();
    record.status = proto::MESSAGE_STATUS_SENT;
    record.created_at = now;
    record.updated_at = now;
    std::string payload = MessageKafkaPayload::serializeMessageData(toMessageData(record));

    {
        auto conn = mysql_pool_->acquire();
        if (!conn) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::DB_ERROR); return grpc::Status::OK; }
        MySqlTransaction tx(*conn);
        if (!tx.active()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::DB_ERROR); return grpc::Status::OK; }
        if (!message_dao_->insertMessage(*conn, record)) {
            tx.rollback();
            auto existing = message_dao_->getMessageByClientSequence(request->from_user_id(), request->client_sequence_id());
            if (existing.has_value()) {
                fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "DUPLICATED_REQUEST_REPLAYED");
                response->set_message_id(existing->message_id);
                response->set_server_timestamp(existing->created_at);
                return grpc::Status::OK;
            }
            fillResponse(response->mutable_response(), request->request_id(), ErrorCode::MESSAGE_PERSIST_FAILED);
            return grpc::Status::OK;
        }
        if (!conversation_dao_->upsertGroupConversationForMembers(*conn, conversation_id, request->group_id(), request->from_user_id(), members, message_id, ConversationServiceHelper::buildPreview(request->content()), now)) {
            fillResponse(response->mutable_response(), request->request_id(), ErrorCode::DB_ERROR);
            return grpc::Status::OK;
        }
        auto event = makeOutboxEvent(message_id, "message", message_id, options_.topic_group, std::to_string(conversation_id), payload, now);
        if (!outbox_dao_->insertEvent(*conn, event)) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OUTBOX_PUBLISH_FAILED); return grpc::Status::OK; }
        if (!tx.commit()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::DB_ERROR); return grpc::Status::OK; }
    }
    if (request->client_sequence_id() != 0 && !message_deduplicator_->markMessage(request->from_user_id(), request->client_sequence_id(), message_id)) {
        LOG_WARN("message dedup mark failed after group message commit request_id=" + request->request_id() +
                 " user_id=" + std::to_string(request->from_user_id()) +
                 " client_sequence_id=" + std::to_string(request->client_sequence_id()) +
                 " message_id=" + std::to_string(message_id));
    }
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    response->set_message_id(message_id);
    response->set_server_timestamp(now);
    MetricsRegistry::instance().counter("nebula_message_send_group_success_total", "MessageService successful group-message sends").inc();
    return grpc::Status::OK;
}

grpc::Status MessageServiceImpl::ListConversationMessages(
    grpc::ServerContext* context,
    const proto::ListConversationMessagesRequest* request,
    proto::ListConversationMessagesResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
    if (invalidDeps()) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR);
        return grpc::Status::OK;
    }
    if (request->requester_user_id() == 0 || request->conversation_id() == 0 ||
        !canReadConversation(request->requester_user_id(), request->conversation_id())) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::CONVERSATION_NOT_FOUND);
        return grpc::Status::OK;
    }
    size_t limit = request->page_size() == 0 ? 50 : std::min<size_t>(request->page_size(), 100);
    int64_t before = request->before_time();
    auto messages = message_dao_->listConversationMessages(
        request->conversation_id(), before, request->before_message_id(), limit + 1);
    const bool has_more = messages.size() > limit;
    if (has_more) messages.resize(limit);
    std::reverse(messages.begin(), messages.end());
    for (const auto& message : messages) {
        *response->add_messages() = toMessageData(message);
    }
    if (!messages.empty()) {
        response->set_next_before_time(messages.front().created_at);
        response->set_next_before_message_id(messages.front().message_id);
    }
    response->set_has_more(has_more);
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status MessageServiceImpl::AckMessage(grpc::ServerContext* context, const proto::AckMessageRequest* request, proto::AckMessageResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
    MetricsRegistry::instance().counter("nebula_message_ack_total", "MessageService ack requests").inc();
    if (invalidDeps()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->user_id() == 0 || request->message_id() == 0) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    if (!user_dao_->getUserById(request->user_id()).has_value()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::USER_NOT_FOUND); return grpc::Status::OK; }
    auto record = message_dao_->getMessageById(request->message_id());
    if (!record.has_value()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::MESSAGE_NOT_FOUND); return grpc::Status::OK; }
    if (!canReceiveMessage(request->user_id(), record.value())) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::MESSAGE_PERMISSION_DENIED); return grpc::Status::OK; }
    if (message_receipt_dao_ == nullptr) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR);
        return grpc::Status::OK;
    }
    auto conn = mysql_pool_->acquire();
    if (!conn) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::DB_ERROR);
        return grpc::Status::OK;
    }
    MySqlTransaction tx(*conn);
    int64_t now = TimeUtil::nowMs();
    if (!tx.active() ||
        !message_receipt_dao_->upsertDelivered(*conn, request->message_id(), request->user_id(), now) ||
        !offline_message_dao_->markAsAcked(*conn, request->user_id(), request->message_id()) ||
        !tx.commit()) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::DB_ERROR);
        return grpc::Status::OK;
    }
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    MetricsRegistry::instance().counter("nebula_message_ack_success_total", "MessageService successful acks").inc();
    return grpc::Status::OK;
}

grpc::Status MessageServiceImpl::PullOfflineMessages(grpc::ServerContext* context, const proto::PullOfflineMessagesRequest* request, proto::PullOfflineMessagesResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
    MetricsRegistry::instance().counter("nebula_message_pull_offline_total", "MessageService pull-offline requests").inc();
    if (invalidDeps()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->user_id() == 0) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    if (!user_dao_->getUserById(request->user_id()).has_value()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::USER_NOT_FOUND); return grpc::Status::OK; }
    size_t limit = request->page().page_size() == 0 ? static_cast<size_t>(options_.offline_pull_limit) : request->page().page_size();
    if (limit > static_cast<size_t>(options_.offline_pull_limit)) limit = static_cast<size_t>(options_.offline_pull_limit);
    auto offline = offline_message_dao_->listOfflineMessages(request->user_id(), limit);
    std::vector<uint64_t> delivered;
    auto conn = mysql_pool_->acquire();
    if (!conn) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::DB_ERROR);
        return grpc::Status::OK;
    }
    MySqlTransaction tx(*conn);
    if (!tx.active()) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::DB_ERROR);
        return grpc::Status::OK;
    }
    for (const auto& item : offline) {
        proto::MessageData data;
        if (MessageKafkaPayload::deserializeMessageData(item.payload, &data)) {
            *response->add_messages() = data;
            delivered.push_back(item.message_id);
        } else {
            LOG_ERROR("failed to parse offline message payload message_id=" + std::to_string(item.message_id));
        }
    }
    if (!offline_message_dao_->markBatchAsPulled(*conn, request->user_id(), delivered) || !tx.commit()) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::DB_ERROR);
        response->clear_messages();
        return grpc::Status::OK;
    }
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    response->mutable_page()->set_page(request->page().page());
    response->mutable_page()->set_page_size(static_cast<uint32_t>(limit));
    response->mutable_page()->set_total(response->messages_size());
    MetricsRegistry::instance().counter("nebula_message_pull_offline_success_total", "MessageService successful pull-offline requests").inc();
    return grpc::Status::OK;
}

grpc::Status MessageServiceImpl::MarkMessageRead(grpc::ServerContext* context, const proto::MarkMessageReadRequest* request, proto::CommonResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response)) return grpc::Status::OK;
    MetricsRegistry::instance().counter("nebula_message_mark_read_total", "MessageService mark-message-read requests").inc();
    if (invalidDeps() || message_receipt_dao_ == nullptr) { fillResponse(response, request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->user_id() == 0 || request->message_id() == 0) { fillResponse(response, request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    auto record = message_dao_->getMessageById(request->message_id());
    if (!record.has_value()) { fillResponse(response, request->request_id(), ErrorCode::MESSAGE_NOT_FOUND); return grpc::Status::OK; }
    if (!canReceiveMessage(request->user_id(), record.value())) { fillResponse(response, request->request_id(), ErrorCode::MESSAGE_PERMISSION_DENIED); return grpc::Status::OK; }
    auto conn = mysql_pool_->acquire();
    if (!conn) { fillResponse(response, request->request_id(), ErrorCode::DB_ERROR); return grpc::Status::OK; }
    MySqlTransaction tx(*conn);
    int64_t now = TimeUtil::nowMs();
    if (!tx.active() ||
        !message_receipt_dao_->markRead(*conn, request->message_id(), request->user_id(), now) ||
        !conversation_dao_->recalculateUnread(*conn, request->user_id(), record->conversation_id, now) ||
        !tx.commit()) {
        fillResponse(response, request->request_id(), ErrorCode::DB_ERROR);
        return grpc::Status::OK;
    }
    fillResponse(response, request->request_id(), ErrorCode::OK, "OK");
    MetricsRegistry::instance().counter("nebula_message_mark_read_success_total", "MessageService successful mark-message-read requests").inc();
    return grpc::Status::OK;
}

grpc::Status MessageServiceImpl::MarkConversationRead(grpc::ServerContext* context, const proto::MarkConversationReadRequest* request, proto::CommonResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response)) return grpc::Status::OK;
    MetricsRegistry::instance().counter("nebula_message_mark_conversation_read_total", "MessageService mark-conversation-read requests").inc();
    if (invalidDeps() || message_receipt_dao_ == nullptr) { fillResponse(response, request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->user_id() == 0 || request->conversation_id() == 0 || request->up_to_message_id() == 0) { fillResponse(response, request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    if (!canReadConversation(request->user_id(), request->conversation_id())) { fillResponse(response, request->request_id(), ErrorCode::CONVERSATION_NOT_FOUND); return grpc::Status::OK; }
    auto cursor = message_dao_->getMessageById(request->up_to_message_id());
    if (!cursor.has_value() || cursor->conversation_id != request->conversation_id()) {
        fillResponse(response, request->request_id(), ErrorCode::MESSAGE_NOT_FOUND);
        return grpc::Status::OK;
    }
    int64_t now = TimeUtil::nowMs();
    auto conn = mysql_pool_->acquire();
    if (!conn) { fillResponse(response, request->request_id(), ErrorCode::DB_ERROR); return grpc::Status::OK; }
    MySqlTransaction tx(*conn);
    if (!tx.active() ||
        !message_receipt_dao_->markConversationRead(*conn, request->conversation_id(), request->user_id(),
                                                    request->up_to_message_id(), cursor->created_at, now) ||
        !conversation_dao_->recalculateUnread(*conn, request->user_id(), request->conversation_id(), now) ||
        !tx.commit()) {
        fillResponse(response, request->request_id(), ErrorCode::DB_ERROR);
        return grpc::Status::OK;
    }
    fillResponse(response, request->request_id(), ErrorCode::OK, "OK");
    MetricsRegistry::instance().counter("nebula_message_mark_conversation_read_success_total", "MessageService successful mark-conversation-read requests").inc();
    return grpc::Status::OK;
}

grpc::Status MessageServiceImpl::GetMessageReadState(grpc::ServerContext* context, const proto::GetMessageReadStateRequest* request, proto::GetMessageReadStateResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
    if (message_receipt_dao_ == nullptr) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->message_id() == 0 || request->requester_user_id() == 0) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    auto record = message_dao_->getMessageById(request->message_id());
    if (!record.has_value()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::MESSAGE_NOT_FOUND); return grpc::Status::OK; }
    if (record->from_user_id != request->requester_user_id()) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::MESSAGE_PERMISSION_DENIED);
        return grpc::Status::OK;
    }
    for (const auto& state : message_receipt_dao_->getReadState(request->message_id())) {
        auto* out = response->add_states();
        out->set_message_id(state.message_id);
        out->set_user_id(state.user_id);
        out->set_delivered_at(state.delivered_at);
        out->set_read_at(state.read_at);
    }
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status MessageServiceImpl::RecallMessage(grpc::ServerContext* context, const proto::RecallMessageRequest* request, proto::RecallMessageResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
    std::string inbound_trace = RpcMetadata::extractTraceId(context);
    TraceContext::Scope trace(TraceContext::ensureTraceId(inbound_trace.empty() ? request->request_id() : inbound_trace));
    MetricsRegistry::instance().counter("nebula_message_recall_total", "MessageService recall requests").inc();
    if (invalidDeps()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR); return grpc::Status::OK; }
    if (request->user_id() == 0 || request->message_id() == 0) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT); return grpc::Status::OK; }
    auto record = message_dao_->getMessageById(request->message_id());
    if (!record.has_value()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::MESSAGE_NOT_FOUND); return grpc::Status::OK; }
    if (record->from_user_id != request->user_id()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::MESSAGE_RECALL_PERMISSION_DENIED); return grpc::Status::OK; }
    if (record->recalled) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::MESSAGE_ALREADY_RECALLED); return grpc::Status::OK; }
    int64_t now = TimeUtil::nowMs();
    if (now - record->created_at > static_cast<int64_t>(options_.recall_window_seconds) * 1000) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::MESSAGE_RECALL_TIMEOUT);
        return grpc::Status::OK;
    }
    record->recalled = true;
    record->recalled_at = now;
    record->status = proto::MESSAGE_STATUS_RECALLED;
    std::string payload = MessageKafkaPayload::serializeMessageData(toMessageData(*record));
    auto fillRecallUpdateFailure = [&]() {
        auto latest = message_dao_->getMessageById(request->message_id());
        fillResponse(response->mutable_response(),
                     request->request_id(),
                     latest.has_value() && latest->recalled ? ErrorCode::MESSAGE_ALREADY_RECALLED : ErrorCode::DB_ERROR);
    };
    auto conn = mysql_pool_->acquire();
    if (!conn) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::DB_ERROR); return grpc::Status::OK; }
    MySqlTransaction tx(*conn);
    if (!tx.active()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::DB_ERROR); return grpc::Status::OK; }
    if (!message_dao_->recallMessage(*conn, request->message_id(), now)) { tx.rollback(); fillRecallUpdateFailure(); return grpc::Status::OK; }
    OutboxEvent event = makeOutboxEvent(message_id_generator_->nextId(), "message_recall", request->message_id(),
                                        record->group_id == 0 ? options_.topic_single : options_.topic_group,
                                        std::to_string(record->conversation_id), payload, now);
    if (!outbox_dao_->insertEvent(*conn, event)) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OUTBOX_PUBLISH_FAILED); return grpc::Status::OK; }
    if (!conversation_dao_->recalculateUnreadForConversation(*conn, record->conversation_id, now)) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::DB_ERROR);
        return grpc::Status::OK;
    }
    if (!conversation_dao_->markLastMessageRecalled(*conn, record->conversation_id, record->message_id, now)) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::DB_ERROR);
        return grpc::Status::OK;
    }
    if (!tx.commit()) { fillResponse(response->mutable_response(), request->request_id(), ErrorCode::DB_ERROR); return grpc::Status::OK; }
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    response->set_message_id(request->message_id());
    response->set_recalled_at(now);
    MetricsRegistry::instance().counter("nebula_message_recall_success_total", "MessageService successful recalls").inc();
    return grpc::Status::OK;
}

}  // namespace nebula
