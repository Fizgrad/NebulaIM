#pragma once

#include "common/config/Config.h"

#include <mutex>
#include <string>

#if defined(NEBULA_ENABLE_GRPC)
#include <grpcpp/grpcpp.h>
#else
namespace grpc {
class ClientContext;
class ServerContext;
}
#endif

namespace nebula {

class InternalRpcAuth {
public:
    static InternalRpcAuth& instance();

    void configureFromConfig(const Config& config);
    bool enabled() const;
    void inject(grpc::ClientContext* context) const;
    bool authorize(const grpc::ServerContext* context) const;

private:
    InternalRpcAuth() = default;

    static std::string sha256Hex(const std::string& value);

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string token_;
    std::string token_sha256_hex_;
};

}  // namespace nebula
