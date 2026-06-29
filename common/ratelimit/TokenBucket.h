#pragma once

#include <chrono>
#include <cstddef>
#include <mutex>

namespace nebula {

class TokenBucket {
public:
    TokenBucket(size_t capacity = 1, double refill_rate_per_sec = 1.0);

    bool allow(size_t tokens = 1);
    size_t capacity() const;
    double refillRatePerSecond() const;

private:
    void refillLocked(std::chrono::steady_clock::time_point now);

private:
    mutable std::mutex mutex_;
    double capacity_;
    double tokens_;
    double refill_rate_per_sec_;
    std::chrono::steady_clock::time_point last_refill_;
};

}  // namespace nebula
