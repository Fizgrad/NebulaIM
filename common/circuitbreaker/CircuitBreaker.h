#pragma once

#include <chrono>
#include <mutex>
#include <string>

namespace nebula {

enum class CircuitBreakerState {
    CLOSED,
    OPEN,
    HALF_OPEN
};

class CircuitBreaker {
public:
    CircuitBreaker(size_t failure_threshold = 5, int64_t open_timeout_ms = 30000);

    bool allowRequest();
    void recordSuccess();
    void recordFailure();
    CircuitBreakerState state() const;
    size_t failureCount() const;

private:
    int64_t nowMs() const;

private:
    mutable std::mutex mutex_;
    size_t failure_threshold_;
    int64_t open_timeout_ms_;
    CircuitBreakerState state_;
    size_t failure_count_;
    int64_t opened_at_ms_;
};

std::string circuitBreakerStateToString(CircuitBreakerState state);

}  // namespace nebula
