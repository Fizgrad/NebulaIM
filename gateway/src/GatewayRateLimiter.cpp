#include "GatewayRateLimiter.h"

namespace nebula {

GatewayRateLimiter::GatewayRateLimiter(RateLimitConfig config) : limiter_(config) {}

bool GatewayRateLimiter::allowPacket(const std::string& connection_id) {
    return limiter_.allowPacket(connection_id);
}

bool GatewayRateLimiter::allowLogin(const std::string& ip) {
    return limiter_.allowLogin(ip);
}

bool GatewayRateLimiter::allowMessage(uint64_t user_id) {
    return limiter_.allowMessage(user_id);
}

void GatewayRateLimiter::clearConnection(const std::string& connection_id) {
    limiter_.clearConnection(connection_id);
}

}  // namespace nebula
