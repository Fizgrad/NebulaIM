#include "common/config/Config.h"
#include "common/auth/PasswordHasher.h"
#include "common/dao/MessageDao.h"
#include "common/dao/UserDao.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/kafka/KafkaProducer.h"
#include "common/log/Logger.h"
#include "common/redis/RedisClient.h"
#include "common/utils/TimeUtil.h"

#include <iostream>

int main() {
    nebula::Config config;
    if (!config.loadFromFile("config/nebula.conf")) {
        LOG_ERROR("failed to load config/nebula.conf");
        return 1;
    }

    nebula::MySqlConfig mysql;
    mysql.host = config.getString("mysql.host", mysql.host);
    mysql.port = config.getInt("mysql.port", mysql.port);
    mysql.user = config.getString("mysql.user", mysql.user);
    mysql.password = config.getString("mysql.password", mysql.password);
    mysql.database = config.getString("mysql.database", mysql.database);

    nebula::MySqlConnectionPool pool;
    if (!pool.init(mysql, static_cast<size_t>(config.getInt("mysql.pool_size", 4)))) {
        LOG_ERROR("MySQL init failed");
        return 1;
    }
    LOG_INFO("MySQL pool initialized");

    nebula::RedisConfig redis_config;
    redis_config.host = config.getString("redis.host", redis_config.host);
    redis_config.port = config.getInt("redis.port", redis_config.port);
    redis_config.timeout_ms = config.getInt("redis.timeout_ms", redis_config.timeout_ms);
    redis_config.password = config.getString("redis.password", redis_config.password);

    nebula::RedisClient redis;
    if (!redis.connect(redis_config)) {
        LOG_ERROR("Redis init failed: " + redis.lastError());
        return 1;
    }
    LOG_INFO("Redis connected");

    nebula::KafkaProducerConfig kafka_config;
    kafka_config.brokers = config.getString("kafka.brokers", kafka_config.brokers);
    kafka_config.client_id = config.getString("kafka.producer.client_id", kafka_config.client_id);
    nebula::KafkaProducer producer;
    if (!producer.init(kafka_config)) {
        LOG_ERROR("Kafka producer init failed");
        return 1;
    }
    LOG_INFO("Kafka producer initialized");

    int64_t now = nebula::TimeUtil::nowMs();
    nebula::User user;
    user.username = "storage_demo_" + std::to_string(now);
    user.password_hash = nebula::PasswordHasher::hashPassword("password123");
    user.nickname = "Storage Demo";
    user.created_at = now;
    user.updated_at = now;

    nebula::UserDao user_dao(pool);
    uint64_t user_id = 0;
    if (!user_dao.createUser(user, &user_id)) {
        LOG_ERROR("create user failed");
        return 1;
    }
    LOG_INFO("created user id=" + std::to_string(user_id));

    std::string token = "demo-token-" + std::to_string(user_id);
    std::string device_id = "storage-demo-device";
    std::string connection_id = "storage-demo-conn-" + std::to_string(user_id);
    redis.setEx("nebula:token:" + token, 3600, std::to_string(user_id));
    redis.sadd("nebula:user:devices:" + std::to_string(user_id), device_id);
    redis.expire("nebula:user:devices:" + std::to_string(user_id), 60);
    redis.setEx("nebula:user:online:" + std::to_string(user_id) + ":" + device_id, 60, "gateway-1");
    redis.setEx("nebula:user:conn:" + std::to_string(user_id) + ":" + device_id, 60, connection_id);
    LOG_INFO("wrote Redis token and device-scoped online status");

    nebula::MessageRecord message;
    message.message_id = static_cast<uint64_t>(now);
    message.conversation_id = 80001;
    message.from_user_id = user_id;
    message.to_user_id = 10002;
    message.content = "storage demo message";
    message.created_at = now;
    message.updated_at = now;
    nebula::MessageDao message_dao(pool);
    if (!message_dao.insertMessage(message)) {
        LOG_ERROR("insert message failed");
        return 1;
    }
    LOG_INFO("inserted message id=" + std::to_string(message.message_id));

    if (!producer.produce("nebula.message.single", std::to_string(message.conversation_id), message.content)) {
        LOG_ERROR("Kafka produce failed");
        return 1;
    }
    producer.flush(10000);
    LOG_INFO("sent Kafka message");

    std::cout << "storage demo ok" << std::endl;
    return 0;
}
