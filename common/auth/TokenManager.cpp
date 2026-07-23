#include "common/auth/TokenManager.h"

#include "common/utils/TimeUtil.h"

#include <iomanip>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sstream>

namespace nebula {

TokenManager::TokenManager(int ttl_seconds)
    : ttl_seconds_(ttl_seconds > 0 ? ttl_seconds : 7 * 24 * 3600) {}

std::string TokenManager::generateToken(uint64_t user_id) {
    (void)user_id;
    return randomHex(32);
}

std::string TokenManager::tokenKey(const std::string& token) const {
    return "nebula:token:" + sha256Hex(token);
}

std::string TokenManager::tokenHash(const std::string& token) {
    return sha256Hex(token);
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

std::string TokenManager::sha256Hex(const std::string& input) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx == nullptr) {
        return "";
    }
    bool ok = EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1 &&
              EVP_DigestUpdate(ctx, input.data(), input.size()) == 1 &&
              EVP_DigestFinal_ex(ctx, digest, &digest_len) == 1;
    EVP_MD_CTX_free(ctx);
    if (!ok) {
        return "";
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < digest_len; ++i) {
        oss << std::setw(2) << static_cast<int>(digest[i]);
    }
    return oss.str();
}

}  // namespace nebula
