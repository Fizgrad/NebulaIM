#include "common/config/Config.h"
#if defined(NEBULA_ENABLE_STORAGE)
#include "common/config/StorageConfig.h"
#include <cstdlib>
#endif

#include <cassert>
#include <string>

int main() {
    nebula::Config config;
    assert(config.loadFromFile("config/nebula.conf"));

    assert(config.has("server.host"));
    assert(config.getString("server.host") == "0.0.0.0");
    assert(config.getInt("server.port") == 9000);
    assert(config.getInt64("admin.cleanup.message_receipt_retention_ms") == 7776000000LL);
    assert(config.getBool("log.console") == true);
    assert(config.getString("missing.key", "fallback") == "fallback");
    assert(config.getInt("missing.int", 42) == 42);
    assert(config.getBool("missing.bool", true) == true);
    assert(!config.has("not.exists"));

#if defined(NEBULA_ENABLE_STORAGE)
    assert(::setenv("NEBULA_TEST_MYSQL_USER", "config-test-user", 1) == 0);
    assert(::setenv("NEBULA_TEST_MYSQL_PASSWORD", "config-test-password", 1) == 0);
    assert(::setenv("NEBULA_TEST_REDIS_PASSWORD", "config-test-redis", 1) == 0);
    const auto mysql = nebula::loadMySqlConfig(config);
    const auto redis = nebula::loadRedisConfig(config);
    assert(mysql.user == "config-test-user");
    assert(mysql.password == "config-test-password");
    assert(redis.password == "config-test-redis");
    ::unsetenv("NEBULA_TEST_MYSQL_USER");
    ::unsetenv("NEBULA_TEST_MYSQL_PASSWORD");
    ::unsetenv("NEBULA_TEST_REDIS_PASSWORD");
#endif

    return 0;
}
