#include "common/config/Config.h"
#include "common/rpc/GrpcTlsCredentials.h"

#include <cassert>
#include <cstdio>
#include <fstream>

int main() {
    const char* conf_path = "build/test_grpc_tls_config.conf";
    const char* cert_path = "build/test_grpc_tls_cert.pem";
    const char* key_path = "build/test_grpc_tls_key.pem";

    {
        std::ofstream cert(cert_path);
        cert << "-----BEGIN CERTIFICATE-----\nMIIB\n-----END CERTIFICATE-----\n";
        std::ofstream key(key_path);
        key << "-----BEGIN PRIVATE KEY-----\nMIIB\n-----END PRIVATE KEY-----\n";
        std::ofstream conf(conf_path);
        conf << "grpc.tls.enabled=true\n";
        conf << "grpc.tls.ca_cert_path=" << cert_path << "\n";
        conf << "grpc.tls.server_cert_path=" << cert_path << "\n";
        conf << "grpc.tls.server_key_path=" << key_path << "\n";
        conf << "grpc.tls.client_cert_path=" << cert_path << "\n";
        conf << "grpc.tls.client_key_path=" << key_path << "\n";
        conf << "grpc.tls.require_client_auth=true\n";
        conf << "grpc.tls.target_name_override=nebula.local\n";
    }

    nebula::Config config;
    assert(config.loadFromFile(conf_path));
    nebula::GrpcTlsConfig tls = nebula::GrpcTlsCredentials::fromConfig(config);
    assert(tls.enabled);
    assert(tls.require_client_auth);
    assert(tls.ca_cert_path == cert_path);
    assert(tls.server_cert_path == cert_path);
    assert(tls.server_key_path == key_path);
    assert(tls.client_cert_path == cert_path);
    assert(tls.client_key_path == key_path);
    assert(tls.target_name_override == "nebula.local");
    assert(nebula::GrpcTlsCredentials::validateForServer(tls));
    assert(nebula::GrpcTlsCredentials::validateForClient(tls));

    tls.server_key_path = "build/missing.key";
    std::string error;
    assert(!nebula::GrpcTlsCredentials::validateForServer(tls, &error));
    assert(!error.empty());

    std::remove(conf_path);
    std::remove(cert_path);
    std::remove(key_path);
    return 0;
}
