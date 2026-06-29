#pragma once

#include "common/ratelimit/RateLimiter.h"

namespace nebula {

class GatewayRateLimiter {
public:
    explicit GatewayRateLimiter(RateLimitConfig config = {});

    bool allowPacket(const std::string& connection_id);
    bool allowLogin(const std::string& ip);
    bool allowMessage(uint64_t user_id);
    void clearConnection(const std::string& connection_id);

private:
    RateLimiter limiter_;
};

}  // namespace nebula
