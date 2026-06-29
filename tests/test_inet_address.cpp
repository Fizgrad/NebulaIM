#include "common/net/InetAddress.h"

#include <cassert>

int main() {
    nebula::InetAddress addr(9000, "127.0.0.1");
    assert(addr.toIp() == "127.0.0.1");
    assert(addr.toPort() == 9000);
    assert(addr.toIpPort() == "127.0.0.1:9000");
    return 0;
}
