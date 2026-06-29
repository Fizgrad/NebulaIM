#include "common/tls/TlsContext.h"

#include <openssl/err.h>
#include <openssl/ssl.h>

namespace nebula {

namespace {
std::string opensslError() {
    unsigned long err = ERR_get_error();
    if (err == 0) return "unknown openssl error";
    char buffer[256];
    ERR_error_string_n(err, buffer, sizeof(buffer));
    return buffer;
}
}

TlsContext::TlsContext(SSL_CTX* ctx) : ctx_(ctx) {}

TlsContext::~TlsContext() {
    if (ctx_ != nullptr) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
    }
}

TlsContext::Ptr TlsContext::createServer(const TlsConfig& config, std::string* error) {
    if (!config.enabled) return nullptr;
    if (!TlsConfigLoader::validateServerConfig(config, error)) return nullptr;

    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    if (ctx == nullptr) {
        if (error != nullptr) *error = "SSL_CTX_new failed: " + opensslError();
        return nullptr;
    }

    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION);

    if (SSL_CTX_use_certificate_chain_file(ctx, config.cert_path.c_str()) != 1) {
        if (error != nullptr) *error = "failed to load TLS certificate: " + opensslError();
        SSL_CTX_free(ctx);
        return nullptr;
    }
    if (SSL_CTX_use_PrivateKey_file(ctx, config.key_path.c_str(), SSL_FILETYPE_PEM) != 1) {
        if (error != nullptr) *error = "failed to load TLS private key: " + opensslError();
        SSL_CTX_free(ctx);
        return nullptr;
    }
    if (SSL_CTX_check_private_key(ctx) != 1) {
        if (error != nullptr) *error = "TLS certificate and private key do not match";
        SSL_CTX_free(ctx);
        return nullptr;
    }
    if (!config.ca_cert_path.empty()) {
        if (SSL_CTX_load_verify_locations(ctx, config.ca_cert_path.c_str(), nullptr) != 1) {
            if (error != nullptr) *error = "failed to load TLS CA certificate: " + opensslError();
            SSL_CTX_free(ctx);
            return nullptr;
        }
    }
    if (config.require_client_auth) {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
    } else {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
    }
    return Ptr(new TlsContext(ctx));
}

SSL* TlsContext::newSsl(int fd) const {
    if (ctx_ == nullptr) return nullptr;
    SSL* ssl = SSL_new(ctx_);
    if (ssl == nullptr) return nullptr;
    SSL_set_fd(ssl, fd);
    SSL_set_accept_state(ssl);
    return ssl;
}

bool TlsContext::enabled() const {
    return ctx_ != nullptr;
}

}  // namespace nebula
