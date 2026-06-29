#pragma once

#include "common/tls/TlsConfig.h"

#include <memory>
#include <string>

using ssl_ctx_st = struct ssl_ctx_st;
using ssl_st = struct ssl_st;

namespace nebula {

class TlsContext {
public:
    using Ptr = std::shared_ptr<TlsContext>;

    ~TlsContext();

    TlsContext(const TlsContext&) = delete;
    TlsContext& operator=(const TlsContext&) = delete;

    static Ptr createServer(const TlsConfig& config, std::string* error = nullptr);

    ssl_st* newSsl(int fd) const;
    bool enabled() const;

private:
    explicit TlsContext(ssl_ctx_st* ctx);

private:
    ssl_ctx_st* ctx_ = nullptr;
};

}  // namespace nebula
