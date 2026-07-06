#include "common/rpc/InternalRpcAuth.h"

#include "common/log/Logger.h"
#include "common/rpc/RpcMetadata.h"

#include <openssl/evp.h>

#include <iomanip>
#include <sstream>

namespace nebula {

InternalRpcAuth& InternalRpcAuth::instance() {
    static InternalRpcAuth auth;
    return auth;
}

void InternalRpcAuth::configureFromConfig(const Config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    enabled_ = config.getBool("internal_rpc.auth.enabled", false);
    token_ = config.getString("internal_rpc.auth.token", "");
    token_sha256_hex_ = config.getString("internal_rpc.auth.token_sha256", "");
    if (enabled_ && token_.empty() && token_sha256_hex_.empty()) {
        LOG_WARN("internal RPC auth enabled without token or token hash; all internal RPC requests will be rejected");
    }
}

bool InternalRpcAuth::enabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return enabled_;
}

void InternalRpcAuth::inject(grpc::ClientContext* context) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!enabled_ || token_.empty()) return;
    RpcMetadata::injectInternalToken(context, token_);
}

bool InternalRpcAuth::authorize(const grpc::ServerContext* context) const {
    std::string token;
    std::string token_hash;
    bool enabled = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled = enabled_;
        token = token_;
        token_hash = token_sha256_hex_;
    }
    if (!enabled) return true;

    const std::string incoming = RpcMetadata::extractInternalToken(context);
    if (incoming.empty()) return false;
    if (!token_hash.empty()) return sha256Hex(incoming) == token_hash;
    return !token.empty() && incoming == token;
}

std::string InternalRpcAuth::sha256Hex(const std::string& value) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx == nullptr) return "";
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, value.data(), value.size()) != 1 ||
        EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    EVP_MD_CTX_free(ctx);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < digest_len; ++i) {
        oss << std::setw(2) << static_cast<int>(digest[i]);
    }
    return oss.str();
}

}  // namespace nebula
