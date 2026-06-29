#include "common/ratelimit/RateLimiter.h"

namespace nebula {

RateLimiter::RateLimiter(RateLimitConfig config) : config_(config) {}

bool RateLimiter::allowLogin(const std::string& ip) {
    if (!config_.enabled) return true;
    return allow("login:" + ip, config_.login_per_minute, static_cast<double>(config_.login_per_minute) / 60.0);
}

bool RateLimiter::allowMessage(uint64_t user_id) {
    if (!config_.enabled) return true;
    return allow("message:" + std::to_string(user_id), config_.message_per_second, static_cast<double>(config_.message_per_second));
}

bool RateLimiter::allowPacket(const std::string& connection_id) {
    if (!config_.enabled) return true;
    return allow("packet:" + connection_id, config_.connection_packet_per_second, static_cast<double>(config_.connection_packet_per_second));
}

void RateLimiter::clearConnection(const std::string& connection_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    buckets_.erase("packet:" + connection_id);
}

bool RateLimiter::allow(const std::string& key, size_t capacity, double refill_rate_per_sec) {
    std::unique_ptr<TokenBucket>* bucket = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = buckets_.find(key);
        if (it == buckets_.end()) {
            it = buckets_.emplace(key, std::make_unique<TokenBucket>(capacity, refill_rate_per_sec)).first;
        }
        bucket = &it->second;
    }
    return (*bucket)->allow();
}

}  // namespace nebula
