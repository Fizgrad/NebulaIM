#include "common/push/PushRetryManager.h"

#include "common/redis/RedisClient.h"

namespace nebula {

PushRetryManager::PushRetryManager(RedisClient* redis_client, int max_retry_count, int ttl_seconds)
    : redis_client_(redis_client), max_retry_count_(max_retry_count > 0 ? max_retry_count : 3), ttl_seconds_(ttl_seconds > 0 ? ttl_seconds : 86400) {}

int PushRetryManager::increaseRetryCount(uint64_t message_id, uint64_t user_id) {
    if (redis_client_ == nullptr) return max_retry_count_ + 1;
    std::string key = retryKey(message_id, user_id);
    int64_t count = redis_client_->incr(key);
    if (count == 1) redis_client_->expire(key, ttl_seconds_);
    return static_cast<int>(count);
}

bool PushRetryManager::shouldRetry(uint64_t message_id, uint64_t user_id) {
    if (redis_client_ == nullptr) return false;
    auto value = redis_client_->get(retryKey(message_id, user_id));
    if (!value.has_value()) return true;
    try { return std::stoi(value.value()) < max_retry_count_; } catch (...) { return false; }
}

bool PushRetryManager::clearRetry(uint64_t message_id, uint64_t user_id) {
    return redis_client_ != nullptr && redis_client_->del(retryKey(message_id, user_id));
}

int PushRetryManager::maxRetryCount() const { return max_retry_count_; }

std::string PushRetryManager::retryKey(uint64_t message_id, uint64_t user_id) const {
    return "nebula:push:retry:" + std::to_string(message_id) + ":" + std::to_string(user_id);
}

}  // namespace nebula
