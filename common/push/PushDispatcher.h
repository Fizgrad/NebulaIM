#pragma once

#include "common.pb.h"
#include "common/push/OnlineStatusManager.h"

#include <string>

namespace nebula {

class GatewayClientManager;
class GroupDao;
class KafkaProducer;
class OfflineMessageDao;
class PushRetryManager;

struct PushOptions {
    std::string topic_retry = "nebula.message.retry";
    std::string topic_dlq = "nebula.message.dlq";
    bool skip_sender_for_group = true;
};

struct PushResult {
    uint32_t success_count = 0;
    uint32_t failed_count = 0;
};

class PushDispatcher {
public:
    PushDispatcher(OnlineStatusManager* online_status_manager,
                   GatewayClientManager* gateway_client_manager,
                   OfflineMessageDao* offline_message_dao,
                   GroupDao* group_dao,
                   KafkaProducer* kafka_producer,
                   PushRetryManager* retry_manager,
                   PushOptions options);

    bool pushToUser(const std::string& request_id, uint64_t user_id, const proto::MessageData& message);
    PushResult pushToGroup(const std::string& request_id, uint64_t group_id, const proto::MessageData& message);
    bool handlePushFailure(const std::string& request_id, uint64_t user_id, const proto::MessageData& message);

private:
    bool deliverOnline(const std::string& request_id, uint64_t user_id, const OnlineStatus& status, const proto::MessageData& message);
    bool saveOffline(uint64_t user_id, const proto::MessageData& message);
    bool sendToRetryTopic(uint64_t user_id, const proto::MessageData& message);
    bool sendToDlqTopic(uint64_t user_id, const proto::MessageData& message);

private:
    OnlineStatusManager* online_status_manager_;
    GatewayClientManager* gateway_client_manager_;
    OfflineMessageDao* offline_message_dao_;
    GroupDao* group_dao_;
    KafkaProducer* kafka_producer_;
    PushRetryManager* retry_manager_;
    PushOptions options_;
};

}  // namespace nebula
