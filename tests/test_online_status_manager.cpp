#include "TestDeps.h"
#include "common/config/Config.h"
#include "common/push/OnlineStatusManager.h"
#include "common/redis/RedisClient.h"

#include <cassert>

int main() {
    nebula::RedisClient client;
    std::string reason;
    if (!nebula::tests::connectRedis(&client, &reason)) return nebula::tests::skip("test_online_status_manager", reason);
    nebula::OnlineStatusManager manager(&client);
    uint64_t uid = 91001;
    assert(manager.setOnline(uid, "device-1", "gateway-1", "conn-1", 60));
    auto status = manager.getOnlineStatus(uid);
    assert(status.online);
    assert(status.gateway_id == "gateway-1");
    assert(status.connection_id == "conn-1");
    assert(status.device_id == "device-1");
    assert(manager.setOffline(uid));
    assert(!manager.getOnlineStatus(uid).online);
    return 0;
}
