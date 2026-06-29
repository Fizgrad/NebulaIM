#include "common/circuitbreaker/CircuitBreakerManager.h"

namespace nebula {

CircuitBreaker& CircuitBreakerManager::getBreaker(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = breakers_.find(name);
    if (it == breakers_.end()) {
        it = breakers_.emplace(name, std::make_unique<CircuitBreaker>()).first;
    }
    return *it->second;
}

bool CircuitBreakerManager::allowRequest(const std::string& name) {
    return getBreaker(name).allowRequest();
}

void CircuitBreakerManager::recordSuccess(const std::string& name) {
    getBreaker(name).recordSuccess();
}

void CircuitBreakerManager::recordFailure(const std::string& name) {
    getBreaker(name).recordFailure();
}

}  // namespace nebula
