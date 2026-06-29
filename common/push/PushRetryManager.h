#pragma once

#include <cstdint>
#include <string>

namespace nebula {

class RedisClient;

class PushRetryManager {
public:
    PushRetryManager(RedisClient* redis_client, int max_retry_count, int ttl_seconds);

    int increaseRetryCount(uint64_t message_id, uint64_t user_id);
    bool shouldRetry(uint64_t message_id, uint64_t user_id);
    bool clearRetry(uint64_t message_id, uint64_t user_id);
    int maxRetryCount() const;

private:
    std::string retryKey(uint64_t message_id, uint64_t user_id) const;

private:
    RedisClient* redis_client_;
    int max_retry_count_;
    int ttl_seconds_;
};

}  // namespace nebula
