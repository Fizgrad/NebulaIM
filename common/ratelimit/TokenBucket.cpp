#include "common/ratelimit/TokenBucket.h"

#include <algorithm>

namespace nebula {

TokenBucket::TokenBucket(size_t capacity, double refill_rate_per_sec)
    : capacity_(capacity == 0 ? 1.0 : static_cast<double>(capacity)),
      tokens_(capacity_),
      refill_rate_per_sec_(refill_rate_per_sec > 0 ? refill_rate_per_sec : 1.0),
      last_refill_(std::chrono::steady_clock::now()) {}

bool TokenBucket::allow(size_t tokens) {
    if (tokens == 0) return true;
    std::lock_guard<std::mutex> lock(mutex_);
    refillLocked(std::chrono::steady_clock::now());
    if (tokens_ < static_cast<double>(tokens)) {
        return false;
    }
    tokens_ -= static_cast<double>(tokens);
    return true;
}

size_t TokenBucket::capacity() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<size_t>(capacity_);
}

double TokenBucket::refillRatePerSecond() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return refill_rate_per_sec_;
}

void TokenBucket::refillLocked(std::chrono::steady_clock::time_point now) {
    const std::chrono::duration<double> elapsed = now - last_refill_;
    if (elapsed.count() <= 0) return;
    tokens_ = std::min(capacity_, tokens_ + elapsed.count() * refill_rate_per_sec_);
    last_refill_ = now;
}

}  // namespace nebula
