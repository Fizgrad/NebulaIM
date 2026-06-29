#include "common/push/GatewayClient.h"

#include <cassert>

int main() {
    nebula::GatewayClient client("127.0.0.1:50055");
    (void)client;
    return 0;
}
