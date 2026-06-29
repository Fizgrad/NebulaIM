#pragma once

#include <cstdint>
#include <string>

namespace nebula {

struct ConnectionContext {
    std::string connection_id;
    uint64_t user_id = 0;
    std::string token;
    std::string device_id;
    std::string platform;
    bool authenticated = false;
    int64_t connected_at_ms = 0;
    int64_t last_active_ms = 0;
    std::string peer_addr;
};

}  // namespace nebula
