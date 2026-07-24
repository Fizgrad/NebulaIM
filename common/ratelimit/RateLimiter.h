#pragma once

#include "common/ratelimit/TokenBucket.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace nebula {

struct RateLimitConfig {
    bool enabled = true;
    size_t login_per_minute = 20;
    size_t message_per_second = 10;
    size_t connection_packet_per_second = 50;
    size_t max_bucket_count = 100000;
    size_t bucket_idle_seconds = 600;
};

class RateLimiter {
public:
    explicit RateLimiter(RateLimitConfig config = {});

    bool allowLogin(const std::string& ip);
    bool allowMessage(uint64_t user_id);
    bool allowPacket(const std::string& connection_id);
    void clearConnection(const std::string& connection_id);

private:
    bool allow(const std::string& key, size_t capacity, double refill_rate_per_sec);
    void pruneLocked(std::chrono::steady_clock::time_point now);

    struct BucketEntry {
        std::shared_ptr<TokenBucket> bucket;
        std::chrono::steady_clock::time_point last_used;
    };

private:
    RateLimitConfig config_;
    std::mutex mutex_;
    std::unordered_map<std::string, BucketEntry> buckets_;
    size_t operation_count_ = 0;
};

}  // namespace nebula
