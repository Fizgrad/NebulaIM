#include "common/gateway/GatewayBackendClients.h"
#include "common/gateway/GatewayOnlineManager.h"
#include "common/gateway/GatewayRouter.h"
#include "common/gateway/ConnectionManager.h"
#include "common/protocol/PacketCodec.h"

#include <cassert>

int main() {
    nebula::ConnectionManager manager("gateway-test");
    nebula::PacketCodec codec;
    nebula::GatewayRouter router(&manager, nullptr, nullptr, &codec);
    (void)router;
    return 0;
}
