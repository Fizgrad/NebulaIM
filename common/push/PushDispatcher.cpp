#include "common/push/PushDispatcher.h"

#include "common/dao/GroupDao.h"
#include "common/dao/OfflineMessageDao.h"
#include "common/kafka/KafkaProducer.h"
#include "common/log/Logger.h"
#include "common/message/MessageKafkaPayload.h"
#include "common/push/GatewayClientManager.h"
#include "common/push/PushRetryManager.h"
#include "common/utils/TimeUtil.h"

namespace nebula {

PushDispatcher::PushDispatcher(OnlineStatusManager* online_status_manager,
                               GatewayClientManager* gateway_client_manager,
                               OfflineMessageDao* offline_message_dao,
                               GroupDao* group_dao,
                               KafkaProducer* kafka_producer,
                               PushRetryManager* retry_manager,
                               PushOptions options)
    : online_status_manager_(online_status_manager), gateway_client_manager_(gateway_client_manager),
      offline_message_dao_(offline_message_dao), group_dao_(group_dao), kafka_producer_(kafka_producer),
      retry_manager_(retry_manager), options_(std::move(options)) {}

bool PushDispatcher::pushToUser(const std::string& request_id, uint64_t user_id, const proto::MessageData& message) {
    if (user_id == 0 || message.message_id() == 0) return false;
    auto statuses = online_status_manager_ ? online_status_manager_->getOnlineStatuses(user_id) : std::vector<OnlineStatus>{};
    LOG_INFO("push to user request_id=" + request_id +
             " user_id=" + std::to_string(user_id) +
             " message_id=" + std::to_string(message.message_id()) +
             " online_devices=" + std::to_string(statuses.size()));
    if (!statuses.empty()) {
        bool delivered = false;
        for (const auto& status : statuses) {
            delivered = deliverOnline(request_id, user_id, status, message) || delivered;
        }
        if (delivered) {
            if (retry_manager_) retry_manager_->clearRetry(message.message_id(), user_id);
            return true;
        }
        return handlePushFailure(request_id, user_id, message);
    }
    return saveOffline(user_id, message);
}

PushResult PushDispatcher::pushToGroup(const std::string& request_id, uint64_t group_id, const proto::MessageData& message) {
    PushResult result;
    if (group_id == 0 || group_dao_ == nullptr) { result.failed_count = 1; return result; }
    auto members = group_dao_->listMembers(group_id);
    for (uint64_t user_id : members) {
        if (options_.skip_sender_for_group && user_id == message.from_user_id()) continue;
        if (pushToUser(request_id, user_id, message)) ++result.success_count;
        else ++result.failed_count;
    }
    return result;
}

bool PushDispatcher::handlePushFailure(const std::string&, uint64_t user_id, const proto::MessageData& message) {
    int count = retry_manager_ ? retry_manager_->increaseRetryCount(message.message_id(), user_id) : 999;
    if (retry_manager_ && count <= retry_manager_->maxRetryCount()) {
        return sendToRetryTopic(user_id, message);
    }
    bool dlq_ok = sendToDlqTopic(user_id, message);
    saveOffline(user_id, message);
    return dlq_ok;
}

bool PushDispatcher::deliverOnline(const std::string& request_id, uint64_t user_id, const OnlineStatus& status, const proto::MessageData& message) {
    if (gateway_client_manager_ == nullptr) return false;
    GatewayClient* client = gateway_client_manager_->getClient(status.gateway_id);
    if (client == nullptr) return false;
    bool ok = client->deliverToConnection(request_id, user_id, status.connection_id, message);
    LOG_INFO("push deliver online request_id=" + request_id +
             " user_id=" + std::to_string(user_id) +
             " device_id=" + status.device_id +
             " gateway_id=" + status.gateway_id +
             " connection_id=" + status.connection_id +
             " message_id=" + std::to_string(message.message_id()) +
             " ok=" + (ok ? "true" : "false"));
    return ok;
}

bool PushDispatcher::saveOffline(uint64_t user_id, const proto::MessageData& message) {
    if (offline_message_dao_ == nullptr) return false;
    OfflineMessage offline;
    offline.user_id = user_id;
    offline.message_id = message.message_id();
    offline.payload = MessageKafkaPayload::serializeMessageData(message);
    offline.status = 0;
    offline.created_at = TimeUtil::nowMs();
    offline.updated_at = offline.created_at;
    return offline_message_dao_->insertOfflineMessage(offline);
}

bool PushDispatcher::sendToRetryTopic(uint64_t user_id, const proto::MessageData& message) {
    if (kafka_producer_ == nullptr) return false;
    std::string payload = MessageKafkaPayload::serializeMessageData(message);
    return kafka_producer_->produce(options_.topic_retry, std::to_string(user_id), payload);
}

bool PushDispatcher::sendToDlqTopic(uint64_t user_id, const proto::MessageData& message) {
    if (kafka_producer_ == nullptr) return false;
    std::string payload = MessageKafkaPayload::serializeMessageData(message);
    bool ok = kafka_producer_->produce(options_.topic_dlq, std::to_string(user_id), payload);
    kafka_producer_->flush(5000);
    return ok;
}

}  // namespace nebula
