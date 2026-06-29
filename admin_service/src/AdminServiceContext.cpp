#include "AdminServiceContext.h"

#include "common/log/Logger.h"

namespace nebula {

bool AdminServiceContext::init(const std::string& config_path) {
    if (!config_.loadFromFile(config_path)) return false;
    std::string host = config_.getString("admin_service.host", "0.0.0.0");
    int port = config_.getInt("admin_service.port", 50057);
    listen_address_ = host + ":" + std::to_string(port);
    admin_auth_ = AdminAuth::fromConfig(config_);
    grpc_tls_config_ = GrpcTlsCredentials::fromConfig(config_);
    cleanup_options_.outbox_published_retention_ms = config_.getInt64("admin.cleanup.outbox_published_retention_ms", cleanup_options_.outbox_published_retention_ms);
    cleanup_options_.offline_delivered_retention_ms = config_.getInt64("admin.cleanup.offline_delivered_retention_ms", cleanup_options_.offline_delivered_retention_ms);
    cleanup_options_.friend_request_retention_ms = config_.getInt64("admin.cleanup.friend_request_retention_ms", cleanup_options_.friend_request_retention_ms);
    cleanup_options_.message_receipt_retention_ms = config_.getInt64("admin.cleanup.message_receipt_retention_ms", cleanup_options_.message_receipt_retention_ms);
    cleanup_options_.cleanup_batch_size = config_.getInt("admin.cleanup.batch_size", cleanup_options_.cleanup_batch_size);

#if defined(NEBULA_ENABLE_STORAGE)
    MySqlConfig mysql;
    mysql.host = config_.getString("mysql.host", mysql.host);
    mysql.port = config_.getInt("mysql.port", mysql.port);
    mysql.user = config_.getString("mysql.user", mysql.user);
    mysql.password = config_.getString("mysql.password", mysql.password);
    mysql.database = config_.getString("mysql.database", mysql.database);
    if (!mysql_pool_.init(mysql, static_cast<size_t>(config_.getInt("mysql.pool_size", 4)))) {
        LOG_ERROR("failed to initialize MySQL pool for AdminService");
        return false;
    }

    redis_client_ = std::make_unique<RedisClient>();
    RedisConfig redis;
    redis.host = config_.getString("redis.host", redis.host);
    redis.port = config_.getInt("redis.port", redis.port);
    redis.timeout_ms = config_.getInt("redis.timeout_ms", redis.timeout_ms);
    redis.password = config_.getString("redis.password", redis.password);
    if (!redis_client_->connect(redis)) {
        LOG_ERROR("failed to connect Redis for AdminService: " + redis_client_->lastError());
        return false;
    }
    kafka_consumer_config_.brokers = config_.getString("kafka.brokers", kafka_consumer_config_.brokers);
    kafka_consumer_config_.group_id = config_.getString("kafka.consumer.group_id", kafka_consumer_config_.group_id);
    kafka_consumer_config_.client_id = config_.getString("kafka.consumer.client_id", kafka_consumer_config_.client_id);
    kafka_consumer_config_.enable_auto_commit = false;
    kafka_topics_ = {
        config_.getString("kafka.topic.single", "nebula.message.single"),
        config_.getString("kafka.topic.group", "nebula.message.group"),
        config_.getString("kafka.topic.retry", "nebula.message.retry"),
        config_.getString("kafka.topic.dlq", "nebula.message.dlq"),
    };
#endif

    return true;
}

std::string AdminServiceContext::listenAddress() const {
    return listen_address_;
}

const AdminAuth& AdminServiceContext::adminAuth() const {
    return admin_auth_;
}

const GrpcTlsConfig& AdminServiceContext::grpcTlsConfig() const {
    return grpc_tls_config_;
}

const AdminCleanupOptions& AdminServiceContext::cleanupOptions() const {
    return cleanup_options_;
}

#if defined(NEBULA_ENABLE_STORAGE)
MySqlConnectionPool* AdminServiceContext::mysqlPool() {
    return &mysql_pool_;
}

RedisClient* AdminServiceContext::redisClient() {
    return redis_client_.get();
}

const KafkaConsumerConfig& AdminServiceContext::kafkaConsumerConfig() const {
    return kafka_consumer_config_;
}

const std::vector<std::string>& AdminServiceContext::kafkaTopics() const {
    return kafka_topics_;
}
#endif

}  // namespace nebula
