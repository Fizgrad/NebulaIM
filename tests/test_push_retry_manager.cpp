#include "TestDeps.h"
#include "common/config/Config.h"
#include "common/push/PushRetryManager.h"
#include "common/redis/RedisClient.h"

#include <cassert>

int main() {
    nebula::RedisClient client;
    std::string reason;
    if (!nebula::tests::connectRedis(&client, &reason)) return nebula::tests::skip("test_push_retry_manager", reason);
    nebula::PushRetryManager retry(&client, 2, 60);
    uint64_t mid = 92001, uid = 92002;
    retry.clearRetry(mid, uid);
    assert(retry.shouldRetry(mid, uid));
    assert(retry.increaseRetryCount(mid, uid) == 1);
    assert(retry.shouldRetry(mid, uid));
    assert(retry.increaseRetryCount(mid, uid) == 2);
    assert(!retry.shouldRetry(mid, uid));
    assert(retry.clearRetry(mid, uid));
    assert(retry.shouldRetry(mid, uid));
    return 0;
}
