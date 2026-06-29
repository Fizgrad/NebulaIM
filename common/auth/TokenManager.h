#pragma once

#include <cstdint>
#include <string>

namespace nebula {

class TokenManager {
public:
    explicit TokenManager(int ttl_seconds = 7 * 24 * 3600);

    std::string generateToken(uint64_t user_id);
    std::string tokenKey(const std::string& token) const;
    int ttlSeconds() const;
    int64_t expireAtMs() const;

private:
    int ttl_seconds_;

    static std::string randomHex(size_t bytes);
};

}  // namespace nebula
