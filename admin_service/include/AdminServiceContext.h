#pragma once

#include "common/config/Config.h"
#include "common/rpc/GrpcTlsCredentials.h"
#include "common/security/AdminAuth.h"

#if defined(NEBULA_ENABLE_STORAGE)
#include "common/db/MySqlConnectionPool.h"
#include "common/kafka/KafkaConsumer.h"
#include "common/redis/RedisClient.h"
#endif

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nebula {

struct AdminCleanupOptions {
    int64_t outbox_published_retention_ms = 7LL * 24 * 3600 * 1000;
    int64_t offline_delivered_retention_ms = 7LL * 24 * 3600 * 1000;
    int64_t friend_request_retention_ms = 30LL * 24 * 3600 * 1000;
    int64_t message_receipt_retention_ms = 90LL * 24 * 3600 * 1000;
    int cleanup_batch_size = 1000;
};

class AdminServiceContext {
public:
    bool init(const std::string& config_path);
    std::string listenAddress() const;
    const AdminAuth& adminAuth() const;
    const GrpcTlsConfig& grpcTlsConfig() const;
    const AdminCleanupOptions& cleanupOptions() const;

#if defined(NEBULA_ENABLE_STORAGE)
    MySqlConnectionPool* mysqlPool();
    RedisClient* redisClient();
    const KafkaConsumerConfig& kafkaConsumerConfig() const;
    const std::vector<std::string>& kafkaTopics() const;
#endif

private:
    Config config_;
    std::string listen_address_;
    AdminAuth admin_auth_;
    GrpcTlsConfig grpc_tls_config_;
    AdminCleanupOptions cleanup_options_;

#if defined(NEBULA_ENABLE_STORAGE)
    MySqlConnectionPool mysql_pool_;
    std::unique_ptr<RedisClient> redis_client_;
    KafkaConsumerConfig kafka_consumer_config_;
    std::vector<std::string> kafka_topics_;
#endif
};

}  // namespace nebula
