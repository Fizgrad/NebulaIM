#pragma once

#include "common/config/Config.h"
#include "common/config/StorageConfig.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/kafka/KafkaConsumer.h"
#include "common/kafka/KafkaProducer.h"
#include "common/redis/RedisClient.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace nebula::tests {

inline std::string envOr(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0' ? value : fallback;
}

inline int envIntOr(const char* name, int fallback) {
    const std::string value = envOr(name, "");
    if (value.empty()) return fallback;
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

inline int skip(const std::string& test_name, const std::string& reason) {
    std::cout << test_name << " skipped: " << reason << "\n";
    return 77;
}

inline bool loadConfig(Config* config, std::string* reason) {
    if (config != nullptr && config->loadFromFile("config/nebula.conf")) return true;
    if (reason != nullptr) *reason = "failed to load config/nebula.conf";
    return false;
}

inline RedisConfig redisConfig(const Config& config) {
    return loadRedisConfig(config);
}

inline MySqlConfig mysqlConfig(const Config& config) {
    return loadMySqlConfig(config);
}

inline KafkaProducerConfig kafkaProducerConfig(const Config& config) {
    KafkaProducerConfig kafka;
    kafka.brokers = envOr("NEBULA_TEST_KAFKA_BROKERS", config.getString("kafka.brokers", kafka.brokers));
    kafka.client_id = config.getString("kafka.producer.client_id", kafka.client_id);
    kafka.delivery_timeout_ms = config.getInt("kafka.producer.delivery_timeout_ms", kafka.delivery_timeout_ms);
    return kafka;
}

inline KafkaConsumerConfig kafkaConsumerConfig(const Config& config) {
    KafkaConsumerConfig kafka;
    kafka.brokers = envOr("NEBULA_TEST_KAFKA_BROKERS", config.getString("kafka.brokers", kafka.brokers));
    kafka.group_id = config.getString("kafka.consumer.group_id", kafka.group_id);
    kafka.client_id = config.getString("kafka.consumer.client_id", kafka.client_id);
    kafka.auto_offset_reset = config.getString("kafka.consumer.auto_offset_reset", kafka.auto_offset_reset);
    kafka.enable_auto_commit = config.getBool("kafka.consumer.enable_auto_commit", kafka.enable_auto_commit);
    kafka.session_timeout_ms = config.getInt("kafka.consumer.session_timeout_ms", kafka.session_timeout_ms);
    kafka.heartbeat_interval_ms = config.getInt("kafka.consumer.heartbeat_interval_ms", kafka.heartbeat_interval_ms);
    kafka.max_poll_interval_ms = config.getInt("kafka.consumer.max_poll_interval_ms", kafka.max_poll_interval_ms);
    kafka.fetch_wait_max_ms = config.getInt("kafka.consumer.fetch_wait_max_ms", kafka.fetch_wait_max_ms);
    return kafka;
}

inline bool connectRedis(RedisClient* client, std::string* reason) {
    Config config;
    if (!loadConfig(&config, reason)) return false;
    if (client == nullptr) {
        if (reason != nullptr) *reason = "redis client is null";
        return false;
    }
    if (!client->connect(redisConfig(config))) {
        if (reason != nullptr) *reason = client->lastError().empty() ? "redis connect failed" : client->lastError();
        return false;
    }
    if (!client->ping()) {
        if (reason != nullptr) *reason = client->lastError().empty() ? "redis ping failed" : client->lastError();
        return false;
    }
    return true;
}

inline bool initMySqlPool(MySqlConnectionPool* pool, size_t pool_size, std::string* reason) {
    Config config;
    if (!loadConfig(&config, reason)) return false;
    if (pool == nullptr) {
        if (reason != nullptr) *reason = "mysql pool is null";
        return false;
    }
    if (!pool->init(mysqlConfig(config), pool_size)) {
        if (reason != nullptr) *reason = "mysql pool init failed";
        return false;
    }
    return true;
}

}  // namespace nebula::tests
