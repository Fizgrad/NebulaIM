#include "common/config/Config.h"
#include "common/gateway/GatewayOnlineManager.h"
#include "common/redis/RedisClient.h"

#include <cassert>

int main() {
    nebula::Config config;
    assert(config.loadFromFile("config/nebula.conf"));
    nebula::RedisConfig redis;
    redis.host = config.getString("redis.host", redis.host);
    redis.port = config.getInt("redis.port", redis.port);
    nebula::RedisClient client;
    assert(client.connect(redis));
    nebula::GatewayOnlineManager manager(&client, "gateway-1", 60);
    assert(manager.setOnline(99101, "conn-99101"));
    assert(manager.refreshOnline(99101, "conn-99101"));
    assert(manager.setOffline(99101, "wrong-conn"));
    auto still = client.get("nebula:user:conn:99101");
    assert(still.has_value());
    assert(manager.setOffline(99101, "conn-99101"));
    assert(!client.get("nebula:user:conn:99101").has_value());
    return 0;
}
