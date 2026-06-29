#include "MessageServiceContext.h"

#include "common/log/Logger.h"
#include "common/trace/TraceManager.h"

namespace nebula {

MessageServiceContext::MessageServiceContext() = default;
MessageServiceContext::~MessageServiceContext() = default;

bool MessageServiceContext::init(const std::string& config_path) {
    if (!config_.loadFromFile(config_path)) {
        LOG_ERROR("failed to load config: " + config_path);
        return false;
    }
    TraceManager::instance().configure(TraceManager::configFrom(config_, "nebula-message-service"));

    MySqlConfig mysql;
    mysql.host = config_.getString("mysql.host", mysql.host);
    mysql.port = config_.getInt("mysql.port", mysql.port);
    mysql.user = config_.getString("mysql.user", mysql.user);
    mysql.password = config_.getString("mysql.password", mysql.password);
    mysql.database = config_.getString("mysql.database", mysql.database);
    if (!mysql_pool_.init(mysql, static_cast<size_t>(config_.getInt("mysql.pool_size", 4)))) {
        LOG_ERROR("failed to initialize MySQL pool for MessageService");
        return false;
    }

    auto redis = std::make_unique<RedisClient>();
    RedisConfig redis_config;
    redis_config.host = config_.getString("redis.host", redis_config.host);
    redis_config.port = config_.getInt("redis.port", redis_config.port);
    redis_config.timeout_ms = config_.getInt("redis.timeout_ms", redis_config.timeout_ms);
    redis_config.password = config_.getString("redis.password", redis_config.password);
    if (!redis->connect(redis_config)) {
        LOG_ERROR("failed to connect Redis: " + redis->lastError());
        return false;
    }

    auto kafka = std::make_unique<KafkaProducer>();
    KafkaProducerConfig kafka_config;
    kafka_config.brokers = config_.getString("kafka.brokers", kafka_config.brokers);
    kafka_config.client_id = config_.getString("kafka.producer.client_id", "nebula-message-service");
    if (!kafka->init(kafka_config)) {
        LOG_ERROR("failed to initialize KafkaProducer");
        return false;
    }

    options_.max_content_length = config_.getInt("message.max_content_length", options_.max_content_length);
    options_.dedup_ttl_seconds = config_.getInt("message.dedup_ttl_seconds", options_.dedup_ttl_seconds);
    options_.offline_pull_limit = config_.getInt("message.offline_pull_limit", options_.offline_pull_limit);
    options_.recall_window_seconds = config_.getInt("message.recall_window_seconds", options_.recall_window_seconds);
    options_.outbox_enabled = config_.getBool("outbox.enabled", options_.outbox_enabled);
    options_.outbox_worker_interval_ms = config_.getInt("outbox.worker_interval_ms", options_.outbox_worker_interval_ms);
    options_.outbox_batch_size = config_.getInt("outbox.batch_size", options_.outbox_batch_size);
    options_.outbox_max_retry_count = config_.getInt("outbox.max_retry_count", options_.outbox_max_retry_count);
    options_.outbox_retry_backoff_ms = config_.getInt("outbox.retry_backoff_ms", options_.outbox_retry_backoff_ms);
    options_.topic_single = config_.getString("kafka.topic.single", options_.topic_single);
    options_.topic_group = config_.getString("kafka.topic.group", options_.topic_group);
    options_.topic_offline = config_.getString("kafka.topic.offline", options_.topic_offline);
    options_.topic_retry = config_.getString("kafka.topic.retry", options_.topic_retry);
    options_.topic_dlq = config_.getString("kafka.topic.dlq", options_.topic_dlq);

    std::string host = config_.getString("message_service.host", "0.0.0.0");
    int port = config_.getInt("message_service.port", 50052);
    listen_address_ = host + ":" + std::to_string(port);
    grpc_tls_config_ = GrpcTlsCredentials::fromConfig(config_);

    user_dao_ = std::make_unique<UserDao>(mysql_pool_);
    group_dao_ = std::make_unique<GroupDao>(mysql_pool_);
    message_dao_ = std::make_unique<MessageDao>(mysql_pool_);
    offline_message_dao_ = std::make_unique<OfflineMessageDao>(mysql_pool_);
    conversation_dao_ = std::make_unique<ConversationDao>(mysql_pool_);
    message_receipt_dao_ = std::make_unique<MessageReceiptDao>(mysql_pool_);
    outbox_dao_ = std::make_unique<OutboxDao>(mysql_pool_);
    redis_client_ = std::move(redis);
    kafka_producer_ = std::move(kafka);
    message_id_generator_ = std::make_unique<MessageIdGenerator>(static_cast<uint16_t>(config_.getInt("message.node_id", 1)));
    message_deduplicator_ = std::make_unique<MessageDeduplicator>(redis_client_.get(), options_.dedup_ttl_seconds);
    if (options_.outbox_enabled) {
        OutboxWorkerOptions worker_options;
        worker_options.worker_interval_ms = options_.outbox_worker_interval_ms;
        worker_options.batch_size = static_cast<size_t>(options_.outbox_batch_size);
        worker_options.max_retry_count = options_.outbox_max_retry_count;
        worker_options.retry_backoff_ms = options_.outbox_retry_backoff_ms;
        worker_options.claim_lease_ms = config_.getInt("outbox.claim_lease_ms", 30000);
        worker_options.dlq_topic = options_.topic_dlq;
        outbox_worker_ = std::make_unique<OutboxWorker>(outbox_dao_.get(), kafka_producer_.get(), worker_options);
        outbox_worker_->start();
    }
    return true;
}

UserDao* MessageServiceContext::userDao() { return user_dao_.get(); }
GroupDao* MessageServiceContext::groupDao() { return group_dao_.get(); }
MessageDao* MessageServiceContext::messageDao() { return message_dao_.get(); }
OfflineMessageDao* MessageServiceContext::offlineMessageDao() { return offline_message_dao_.get(); }
ConversationDao* MessageServiceContext::conversationDao() { return conversation_dao_.get(); }
MessageReceiptDao* MessageServiceContext::messageReceiptDao() { return message_receipt_dao_.get(); }
OutboxDao* MessageServiceContext::outboxDao() { return outbox_dao_.get(); }
MySqlConnectionPool* MessageServiceContext::mysqlPool() { return &mysql_pool_; }
RedisClient* MessageServiceContext::redisClient() { return redis_client_.get(); }
KafkaProducer* MessageServiceContext::kafkaProducer() { return kafka_producer_.get(); }
MessageIdGenerator* MessageServiceContext::messageIdGenerator() { return message_id_generator_.get(); }
MessageDeduplicator* MessageServiceContext::messageDeduplicator() { return message_deduplicator_.get(); }
const MessageServiceOptions& MessageServiceContext::options() const { return options_; }
std::string MessageServiceContext::listenAddress() const { return listen_address_; }
const GrpcTlsConfig& MessageServiceContext::grpcTlsConfig() const { return grpc_tls_config_; }

}  // namespace nebula
