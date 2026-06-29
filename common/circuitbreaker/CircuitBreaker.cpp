#include "common/circuitbreaker/CircuitBreaker.h"

namespace nebula {

CircuitBreaker::CircuitBreaker(size_t failure_threshold, int64_t open_timeout_ms)
    : failure_threshold_(failure_threshold == 0 ? 1 : failure_threshold),
      open_timeout_ms_(open_timeout_ms > 0 ? open_timeout_ms : 30000),
      state_(CircuitBreakerState::CLOSED),
      failure_count_(0),
      opened_at_ms_(0) {}

bool CircuitBreaker::allowRequest() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == CircuitBreakerState::CLOSED) return true;
    if (state_ == CircuitBreakerState::HALF_OPEN) return true;
    if (nowMs() - opened_at_ms_ >= open_timeout_ms_) {
        state_ = CircuitBreakerState::HALF_OPEN;
        return true;
    }
    return false;
}

void CircuitBreaker::recordSuccess() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = CircuitBreakerState::CLOSED;
    failure_count_ = 0;
    opened_at_ms_ = 0;
}

void CircuitBreaker::recordFailure() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++failure_count_;
    if (state_ == CircuitBreakerState::HALF_OPEN || failure_count_ >= failure_threshold_) {
        state_ = CircuitBreakerState::OPEN;
        opened_at_ms_ = nowMs();
    }
}

CircuitBreakerState CircuitBreaker::state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

size_t CircuitBreaker::failureCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return failure_count_;
}

int64_t CircuitBreaker::nowMs() const {
    auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now());
    return now.time_since_epoch().count();
}

std::string circuitBreakerStateToString(CircuitBreakerState state) {
    switch (state) {
        case CircuitBreakerState::CLOSED: return "CLOSED";
        case CircuitBreakerState::OPEN: return "OPEN";
        case CircuitBreakerState::HALF_OPEN: return "HALF_OPEN";
    }
    return "UNKNOWN";
}

}  // namespace nebula
