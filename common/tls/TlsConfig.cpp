#include "common/tls/TlsConfig.h"

#include <fstream>

namespace nebula {

namespace {
bool canReadFile(const std::string& path, std::string* error) {
    if (path.empty()) {
        if (error != nullptr) *error = "empty file path";
        return false;
    }
    std::ifstream input(path, std::ios::in | std::ios::binary);
    if (!input.is_open()) {
        if (error != nullptr) *error = "failed to open file: " + path;
        return false;
    }
    return true;
}
}

TlsConfig TlsConfigLoader::fromConfig(const Config& config, const std::string& prefix) {
    TlsConfig tls;
    tls.enabled = config.getBool(prefix + "enabled", false);
    tls.require_client_auth = config.getBool(prefix + "require_client_auth", false);
    tls.ca_cert_path = config.getString(prefix + "ca_cert_path", "");
    tls.cert_path = config.getString(prefix + "cert_path", "");
    tls.key_path = config.getString(prefix + "key_path", "");
    return tls;
}

bool TlsConfigLoader::validateServerConfig(const TlsConfig& config, std::string* error) {
    if (!config.enabled) return true;
    if (!canReadFile(config.cert_path, error)) {
        if (error != nullptr && error->empty()) *error = "server certificate is not readable";
        return false;
    }
    if (!canReadFile(config.key_path, error)) {
        if (error != nullptr && error->empty()) *error = "server private key is not readable";
        return false;
    }
    if (config.require_client_auth && !canReadFile(config.ca_cert_path, error)) {
        if (error != nullptr && error->empty()) *error = "CA certificate is required for client authentication";
        return false;
    }
    return true;
}

}  // namespace nebula
