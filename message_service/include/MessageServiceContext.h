#pragma once

#include "common/config/Config.h"
#include "common/conversation/ConversationDao.h"
#include "common/dao/GroupDao.h"
#include "common/dao/MessageDao.h"
#include "common/dao/MessageReceiptDao.h"
#include "common/dao/OfflineMessageDao.h"
#include "common/dao/UserDao.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/kafka/KafkaProducer.h"
#include "common/message/MessageDeduplicator.h"
#include "common/message/MessageIdGenerator.h"
#include "common/outbox/OutboxDao.h"
#include "common/outbox/OutboxWorker.h"
#include "common/redis/RedisClient.h"
#include "common/rpc/GrpcTlsCredentials.h"

#include <memory>
#include <string>

namespace nebula {

struct MessageServiceOptions {
    int max_content_length = 4096;
    int dedup_ttl_seconds = 86400;
    int offline_pull_limit = 100;
    int recall_window_seconds = 120;
    bool outbox_enabled = true;
    int outbox_worker_interval_ms = 1000;
    int outbox_batch_size = 100;
    int outbox_max_retry_count = 5;
    int outbox_retry_backoff_ms = 1000;
    std::string topic_single = "nebula.message.single";
    std::string topic_group = "nebula.message.group";
    std::string topic_offline = "nebula.message.offline";
    std::string topic_retry = "nebula.message.retry";
    std::string topic_dlq = "nebula.message.dlq";
};

class MessageServiceContext {
public:
    MessageServiceContext();
    ~MessageServiceContext();

    bool init(const std::string& config_path);

    UserDao* userDao();
    GroupDao* groupDao();
    MessageDao* messageDao();
    OfflineMessageDao* offlineMessageDao();
    ConversationDao* conversationDao();
    MessageReceiptDao* messageReceiptDao();
    OutboxDao* outboxDao();
    MySqlConnectionPool* mysqlPool();
    RedisClient* redisClient();
    KafkaProducer* kafkaProducer();
    MessageIdGenerator* messageIdGenerator();
    MessageDeduplicator* messageDeduplicator();

    const MessageServiceOptions& options() const;
    std::string listenAddress() const;
    const GrpcTlsConfig& grpcTlsConfig() const;

private:
    Config config_;
    MySqlConnectionPool mysql_pool_;

    std::unique_ptr<UserDao> user_dao_;
    std::unique_ptr<GroupDao> group_dao_;
    std::unique_ptr<MessageDao> message_dao_;
    std::unique_ptr<OfflineMessageDao> offline_message_dao_;
    std::unique_ptr<ConversationDao> conversation_dao_;
    std::unique_ptr<MessageReceiptDao> message_receipt_dao_;
    std::unique_ptr<OutboxDao> outbox_dao_;
    std::unique_ptr<OutboxWorker> outbox_worker_;

    std::unique_ptr<RedisClient> redis_client_;
    std::unique_ptr<KafkaProducer> kafka_producer_;

    std::unique_ptr<MessageIdGenerator> message_id_generator_;
    std::unique_ptr<MessageDeduplicator> message_deduplicator_;

    MessageServiceOptions options_;
    std::string listen_address_;
    GrpcTlsConfig grpc_tls_config_;
};

}  // namespace nebula
