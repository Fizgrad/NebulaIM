#include "common/push/GatewayClientManager.h"

#include <cassert>

int main() {
    nebula::GatewayClientManager manager;
    assert(manager.empty());
    assert(manager.getClient("missing") == nullptr);

    manager.addGateway("gateway-a", "127.0.0.1:50055");
    auto* first = manager.getClient("gateway-a");
    assert(first != nullptr);
    assert(!manager.empty());

    manager.addGateway("gateway-a", "127.0.0.1:50056");
    auto* replacement = manager.getClient("gateway-a");
    assert(replacement != nullptr);
    assert(manager.getClient("gateway-b") == nullptr);
    return 0;
}
