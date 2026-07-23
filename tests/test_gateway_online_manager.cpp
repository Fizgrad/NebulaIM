#include "TestDeps.h"
#include "common/config/Config.h"
#include "common/gateway/GatewayOnlineManager.h"
#include "common/redis/RedisClient.h"

#include <cassert>

int main() {
    nebula::RedisClient client;
    std::string reason;
    if (!nebula::tests::connectRedis(&client, &reason)) return nebula::tests::skip("test_gateway_online_manager", reason);
    nebula::GatewayOnlineManager manager(&client, "gateway-1", 60);
    assert(manager.setOnline(99101, "device-99101", "conn-99101"));
    assert(manager.refreshOnline(99101, "device-99101", "conn-99101"));
    assert(manager.setOffline(99101, "device-99101", "wrong-conn"));
    auto still = client.get("nebula:user:conn:99101:device-99101");
    assert(still.has_value());
    assert(manager.setOffline(99101, "device-99101", "conn-99101"));
    assert(!client.get("nebula:user:conn:99101:device-99101").has_value());
    return 0;
}
