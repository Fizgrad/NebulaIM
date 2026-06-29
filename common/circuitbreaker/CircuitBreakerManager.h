#pragma once

#include "common/circuitbreaker/CircuitBreaker.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace nebula {

class CircuitBreakerManager {
public:
    CircuitBreaker& getBreaker(const std::string& name);
    bool allowRequest(const std::string& name);
    void recordSuccess(const std::string& name);
    void recordFailure(const std::string& name);

private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<CircuitBreaker>> breakers_;
};

}  // namespace nebula
