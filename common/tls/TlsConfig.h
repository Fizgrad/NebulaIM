#pragma once

#include "common/config/Config.h"

#include <string>

namespace nebula {

struct TlsConfig {
    bool enabled = false;
    bool require_client_auth = false;
    std::string ca_cert_path;
    std::string cert_path;
    std::string key_path;
};

class TlsConfigLoader {
public:
    static TlsConfig fromConfig(const Config& config, const std::string& prefix);
    static bool validateServerConfig(const TlsConfig& config, std::string* error = nullptr);
};

}  // namespace nebula
