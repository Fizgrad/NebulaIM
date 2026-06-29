#include "common/config/Config.h"
#include "common/push/PushRetryManager.h"
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
    nebula::PushRetryManager retry(&client, 2, 60);
    uint64_t mid = 92001, uid = 92002;
    retry.clearRetry(mid, uid);
    assert(retry.shouldRetry(mid, uid));
    assert(retry.increaseRetryCount(mid, uid) == 1);
    assert(retry.shouldRetry(mid, uid));
    assert(retry.increaseRetryCount(mid, uid) == 2);
    assert(!retry.shouldRetry(mid, uid));
    assert(retry.clearRetry(mid, uid));
    assert(retry.shouldRetry(mid, uid));
    return 0;
}
