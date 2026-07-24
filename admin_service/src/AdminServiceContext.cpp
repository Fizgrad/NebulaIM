#include "AdminServiceContext.h"

#include "common/config/StorageConfig.h"
#include "common/log/Logger.h"
#include "common/rpc/InternalRpcAuth.h"
#include "common/trace/TraceManager.h"

namespace nebula {

bool AdminServiceContext::init(const std::string& config_path) {
    if (!config_.loadFromFile(config_path)) return false;
    InternalRpcAuth::instance().configureFromConfig(config_);
    TraceManager::instance().configure(TraceManager::configFrom(config_, "nebula-admin-service"));
    std::string host = config_.getString("admin_service.host", "0.0.0.0");
    int port = config_.getInt("admin_service.port", 50057);
    listen_address_ = host + ":" + std::to_string(port);
    admin_auth_ = AdminAuth::fromConfig(config_);
    grpc_tls_config_ = GrpcTlsCredentials::fromConfig(config_);
    runtime_config_.admin_auth_enabled = admin_auth_.enabled();
    runtime_config_.grpc_tls_enabled = grpc_tls_config_.enabled;
    runtime_config_.gateway_tls_enabled = config_.getBool("gateway.tls.enabled", false);
    runtime_config_.internal_rpc_auth_enabled = config_.getBool("internal_rpc.auth.enabled", false);
    runtime_config_.trace_enabled = config_.getBool("trace.enabled", false);
    runtime_config_.trace_otlp_endpoint = config_.getString("trace.otlp_endpoint", "");
    runtime_config_.gateway_tcp_host = config_.getString("gateway.tcp.host", "127.0.0.1");
    runtime_config_.gateway_tcp_port = config_.getInt("gateway.tcp.port", 9000);
    runtime_config_.mysql_host = config_.getString("mysql.host", "127.0.0.1");
    runtime_config_.mysql_password = config_.getString("mysql.password", "");
    runtime_config_.redis_host = config_.getString("redis.host", "127.0.0.1");
    runtime_config_.redis_password = config_.getString("redis.password", "");
    runtime_config_.kafka_brokers = config_.getString("kafka.brokers", "127.0.0.1:9092");
    runtime_config_.service_addresses = {
        {"gateway-tcp", runtime_config_.gateway_tcp_host + ":" + std::to_string(runtime_config_.gateway_tcp_port)},
        {"gateway-rpc", config_.getString("gateway.rpc.host", "127.0.0.1") + ":" + std::to_string(config_.getInt("gateway.rpc.port", 50055))},
        {"user-service", config_.getString("user_service.host", "127.0.0.1") + ":" + std::to_string(config_.getInt("user_service.port", 50051))},
        {"relation-service", config_.getString("relation_service.host", "127.0.0.1") + ":" + std::to_string(config_.getInt("relation_service.port", 50053))},
        {"conversation-service", config_.getString("conversation_service.host", "127.0.0.1") + ":" + std::to_string(config_.getInt("conversation_service.port", 50056))},
        {"device-service", config_.getString("device_service.host", "127.0.0.1") + ":" + std::to_string(config_.getInt("device_service.port", 50058))},
        {"message-service", config_.getString("message_service.host", "127.0.0.1") + ":" + std::to_string(config_.getInt("message_service.port", 50052))},
        {"push-service", config_.getString("push_service.host", "127.0.0.1") + ":" + std::to_string(config_.getInt("push_service.port", 50054))},
        {"admin-service", host + ":" + std::to_string(port)},
    };
    cleanup_options_.outbox_published_retention_ms = config_.getInt64("admin.cleanup.outbox_published_retention_ms", cleanup_options_.outbox_published_retention_ms);
    cleanup_options_.offline_acked_retention_ms = config_.getInt64("admin.cleanup.offline_acked_retention_ms", cleanup_options_.offline_acked_retention_ms);
    cleanup_options_.friend_request_retention_ms = config_.getInt64("admin.cleanup.friend_request_retention_ms", cleanup_options_.friend_request_retention_ms);
    cleanup_options_.message_receipt_retention_ms = config_.getInt64("admin.cleanup.message_receipt_retention_ms", cleanup_options_.message_receipt_retention_ms);
    cleanup_options_.cleanup_batch_size = config_.getInt("admin.cleanup.batch_size", cleanup_options_.cleanup_batch_size);

#if defined(NEBULA_ENABLE_STORAGE)
    MySqlConfig mysql = loadMySqlConfig(config_);
    runtime_config_.mysql_host = mysql.host;
    runtime_config_.mysql_password = mysql.password;
    if (!mysql_pool_.init(mysql, static_cast<size_t>(config_.getInt("mysql.pool_size", 4)))) {
        LOG_ERROR("failed to initialize MySQL pool for AdminService");
        return false;
    }

    redis_client_ = std::make_unique<RedisClient>();
    RedisConfig redis = loadRedisConfig(config_);
    runtime_config_.redis_host = redis.host;
    runtime_config_.redis_password = redis.password;
    if (!redis_client_->connect(redis)) {
        LOG_ERROR("failed to connect Redis for AdminService: " + redis_client_->lastError());
        return false;
    }
    kafka_consumer_config_.brokers = config_.getString("kafka.brokers", kafka_consumer_config_.brokers);
    kafka_consumer_config_.group_id = config_.getString("kafka.consumer.group_id", kafka_consumer_config_.group_id);
    kafka_consumer_config_.client_id = config_.getString("kafka.consumer.client_id", kafka_consumer_config_.client_id);
    kafka_consumer_config_.auto_offset_reset = config_.getString("kafka.consumer.auto_offset_reset", kafka_consumer_config_.auto_offset_reset);
    kafka_consumer_config_.enable_auto_commit = false;
    kafka_consumer_config_.session_timeout_ms = config_.getInt("kafka.consumer.session_timeout_ms", kafka_consumer_config_.session_timeout_ms);
    kafka_consumer_config_.heartbeat_interval_ms = config_.getInt("kafka.consumer.heartbeat_interval_ms", kafka_consumer_config_.heartbeat_interval_ms);
    kafka_consumer_config_.max_poll_interval_ms = config_.getInt("kafka.consumer.max_poll_interval_ms", kafka_consumer_config_.max_poll_interval_ms);
    kafka_consumer_config_.fetch_wait_max_ms = config_.getInt("kafka.consumer.fetch_wait_max_ms", kafka_consumer_config_.fetch_wait_max_ms);
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

const AdminRuntimeConfig& AdminServiceContext::runtimeConfig() const {
    return runtime_config_;
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
