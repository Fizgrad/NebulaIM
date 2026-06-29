#include "common/ratelimit/TokenBucket.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    nebula::TokenBucket bucket(2, 10.0);
    assert(bucket.allow());
    assert(bucket.allow());
    assert(!bucket.allow());
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    assert(bucket.allow());
    std::cout << "test_token_bucket passed\n";
    return 0;
}
