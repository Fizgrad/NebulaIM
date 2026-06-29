#include "common/circuitbreaker/CircuitBreaker.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    nebula::CircuitBreaker breaker(2, 50);
    assert(breaker.allowRequest());
    breaker.recordFailure();
    assert(breaker.state() == nebula::CircuitBreakerState::CLOSED);
    breaker.recordFailure();
    assert(breaker.state() == nebula::CircuitBreakerState::OPEN);
    assert(!breaker.allowRequest());
    std::this_thread::sleep_for(std::chrono::milliseconds(70));
    assert(breaker.allowRequest());
    assert(breaker.state() == nebula::CircuitBreakerState::HALF_OPEN);
    breaker.recordSuccess();
    assert(breaker.state() == nebula::CircuitBreakerState::CLOSED);
    std::cout << "test_circuit_breaker passed\n";
    return 0;
}
