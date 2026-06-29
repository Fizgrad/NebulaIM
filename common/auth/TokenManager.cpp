#include "common/auth/TokenManager.h"

#include "common/utils/TimeUtil.h"

#include <iomanip>
#include <openssl/rand.h>
#include <sstream>

namespace nebula {

TokenManager::TokenManager(int ttl_seconds)
    : ttl_seconds_(ttl_seconds > 0 ? ttl_seconds : 7 * 24 * 3600) {}

std::string TokenManager::generateToken(uint64_t user_id) {
    return std::to_string(user_id) + "." + randomHex(32);
}

std::string TokenManager::tokenKey(const std::string& token) const {
    return "nebula:token:" + token;
}

int TokenManager::ttlSeconds() const {
    return ttl_seconds_;
}

int64_t TokenManager::expireAtMs() const {
    return TimeUtil::nowMs() + static_cast<int64_t>(ttl_seconds_) * 1000;
}

std::string TokenManager::randomHex(size_t bytes) {
    std::string data(bytes, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(data.data()), static_cast<int>(data.size())) != 1) {
        return "";
    }
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned char ch : data) {
        oss << std::setw(2) << static_cast<int>(ch);
    }
    return oss.str();
}

}  // namespace nebula
