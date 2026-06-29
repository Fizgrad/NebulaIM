#include "common/config/Config.h"
#include "common/message/MessageDeduplicator.h"
#include "common/redis/RedisClient.h"
#include "common/utils/TimeUtil.h"

#include <cassert>

int main() {
    nebula::Config config;
    assert(config.loadFromFile("config/nebula.conf"));
    nebula::RedisConfig redis;
    redis.host = config.getString("redis.host", redis.host);
    redis.port = config.getInt("redis.port", redis.port);
    redis.timeout_ms = config.getInt("redis.timeout_ms", redis.timeout_ms);
    redis.password = config.getString("redis.password", redis.password);
    nebula::RedisClient client;
    assert(client.connect(redis));

    nebula::MessageDeduplicator dedup(&client, 60);
    uint64_t user_id = static_cast<uint64_t>(nebula::TimeUtil::nowMs());
    uint32_t seq = 12345;
    assert(!dedup.getExistingMessageId(user_id, seq).has_value());
    assert(dedup.markMessage(user_id, seq, 999));
    auto existing = dedup.getExistingMessageId(user_id, seq);
    assert(existing.has_value());
    assert(existing.value() == 999);
    assert(dedup.makeKey(user_id, seq) == "nebula:msg:dedup:" + std::to_string(user_id) + ":" + std::to_string(seq));
    assert(!dedup.getExistingMessageId(user_id, 0).has_value());
    assert(dedup.markMessage(user_id, 0, 1));
    return 0;
}
