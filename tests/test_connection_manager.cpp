#include "common/gateway/ConnectionManager.h"

#include <cassert>

int main() {
    nebula::ConnectionManager manager("gateway-test");
    auto id = manager.addConnection(nullptr, "127.0.0.1:10000");
    assert(!id.empty());
    assert(manager.connectionCount() == 1);
    assert(manager.bindUser(id, 10001, "token", "device-a", "test"));
    assert(manager.onlineUserCount() == 1);
    auto ctx = manager.getContext(id);
    assert(ctx.has_value());
    assert(ctx->authenticated);
    auto contexts = manager.getContextsByUserId(10001);
    assert(contexts.size() == 1);
    assert(contexts.front().device_id == "device-a");
    assert(manager.updateActiveTime(id));
    auto timeout = manager.getTimeoutConnections(ctx->last_active_ms + 10000, 1);
    assert(!timeout.empty());
    manager.removeConnection(id);
    assert(manager.connectionCount() == 0);
    assert(manager.onlineUserCount() == 0);
    return 0;
}
