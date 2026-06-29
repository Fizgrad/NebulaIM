#include "common/config/Config.h"
#include "common/tls/TlsConfig.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>

int main() {
    const char* conf_path = "build/test_gateway_tls_config.conf";
    const char* cert_path = "build/test_gateway_tls_cert.pem";
    const char* key_path = "build/test_gateway_tls_key.pem";

    {
        std::ofstream cert(cert_path);
        cert << "cert";
        std::ofstream key(key_path);
        key << "key";
        std::ofstream conf(conf_path);
        conf << "gateway.tls.enabled=true\n";
        conf << "gateway.tls.cert_path=" << cert_path << "\n";
        conf << "gateway.tls.key_path=" << key_path << "\n";
        conf << "gateway.tls.ca_cert_path=" << cert_path << "\n";
        conf << "gateway.tls.require_client_auth=true\n";
    }

    nebula::Config config;
    assert(config.loadFromFile(conf_path));
    nebula::TlsConfig tls = nebula::TlsConfigLoader::fromConfig(config, "gateway.tls.");
    assert(tls.enabled);
    assert(tls.require_client_auth);
    assert(tls.cert_path == cert_path);
    assert(tls.key_path == key_path);
    assert(tls.ca_cert_path == cert_path);
    assert(nebula::TlsConfigLoader::validateServerConfig(tls));

    tls.key_path = "build/missing_gateway_tls.key";
    std::string error;
    assert(!nebula::TlsConfigLoader::validateServerConfig(tls, &error));
    assert(!error.empty());

    std::remove(conf_path);
    std::remove(cert_path);
    std::remove(key_path);

    std::cout << "test_gateway_tls_config passed\n";
    return 0;
}
