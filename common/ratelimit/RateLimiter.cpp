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
    std::shared_ptr<TokenBucket> bucket;
    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++operation_count_;
        if (operation_count_ % 256 == 0 || buckets_.size() >= config_.max_bucket_count) {
            pruneLocked(now);
        }
        auto it = buckets_.find(key);
        if (it == buckets_.end()) {
            if (buckets_.size() >= config_.max_bucket_count) return false;
            BucketEntry entry;
            entry.bucket = std::make_shared<TokenBucket>(capacity, refill_rate_per_sec);
            entry.last_used = now;
            it = buckets_.emplace(key, std::move(entry)).first;
        }
        it->second.last_used = now;
        bucket = it->second.bucket;
    }
    return bucket->allow();
}

void RateLimiter::pruneLocked(std::chrono::steady_clock::time_point now) {
    const auto idle_limit = std::chrono::seconds(config_.bucket_idle_seconds);
    for (auto it = buckets_.begin(); it != buckets_.end();) {
        if (now - it->second.last_used >= idle_limit) {
            it = buckets_.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace nebula
