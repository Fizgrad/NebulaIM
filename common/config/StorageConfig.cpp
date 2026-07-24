#include "common/config/StorageConfig.h"

#include <cstdlib>
#include <initializer_list>
#include <string>

namespace nebula {

namespace {

std::string firstEnvironmentValue(std::initializer_list<const char*> names, const std::string& fallback) {
    for (const char* name : names) {
        const char* value = std::getenv(name);
        if (value != nullptr && value[0] != '\0') return value;
    }
    return fallback;
}

int firstEnvironmentInt(std::initializer_list<const char*> names, int fallback) {
    const std::string value = firstEnvironmentValue(names, "");
    if (value.empty()) return fallback;
    try {
        size_t parsed = 0;
        const int result = std::stoi(value, &parsed);
        return parsed == value.size() ? result : fallback;
    } catch (...) {
        return fallback;
    }
}

}  // namespace

MySqlConfig loadMySqlConfig(const Config& config) {
    MySqlConfig mysql;
    mysql.host = firstEnvironmentValue({"NEBULA_TEST_MYSQL_HOST", "NEBULA_MYSQL_HOST"}, config.getString("mysql.host", mysql.host));
    mysql.port = firstEnvironmentInt({"NEBULA_TEST_MYSQL_PORT", "NEBULA_MYSQL_PORT"}, config.getInt("mysql.port", mysql.port));
    mysql.user = firstEnvironmentValue({"NEBULA_TEST_MYSQL_USER", "NEBULA_MYSQL_USER"}, config.getString("mysql.user", mysql.user));
    mysql.password =
        firstEnvironmentValue({"NEBULA_TEST_MYSQL_PASSWORD", "NEBULA_MYSQL_PASSWORD"}, config.getString("mysql.password", mysql.password));
    mysql.database =
        firstEnvironmentValue({"NEBULA_TEST_MYSQL_DATABASE", "NEBULA_MYSQL_DATABASE"}, config.getString("mysql.database", mysql.database));
    return mysql;
}

RedisConfig loadRedisConfig(const Config& config) {
    RedisConfig redis;
    redis.host = firstEnvironmentValue({"NEBULA_TEST_REDIS_HOST", "NEBULA_REDIS_HOST"}, config.getString("redis.host", redis.host));
    redis.port = firstEnvironmentInt({"NEBULA_TEST_REDIS_PORT", "NEBULA_REDIS_PORT"}, config.getInt("redis.port", redis.port));
    redis.timeout_ms =
        firstEnvironmentInt({"NEBULA_TEST_REDIS_TIMEOUT_MS", "NEBULA_REDIS_TIMEOUT_MS"}, config.getInt("redis.timeout_ms", redis.timeout_ms));
    redis.password =
        firstEnvironmentValue({"NEBULA_TEST_REDIS_PASSWORD", "NEBULA_REDIS_PASSWORD"}, config.getString("redis.password", redis.password));
    return redis;
}

}  // namespace nebula
