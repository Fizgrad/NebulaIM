#include "common/rpc/GrpcTlsCredentials.h"

#if defined(NEBULA_ENABLE_GRPC)
#include "common/log/Logger.h"
#endif

#include <fstream>
#include <sstream>

namespace nebula {

GrpcTlsConfig GrpcTlsCredentials::fromConfig(const Config& config, const std::string& prefix) {
    GrpcTlsConfig tls;
    tls.enabled = config.getBool(prefix + "enabled", false);
    tls.require_client_auth = config.getBool(prefix + "require_client_auth", false);
    tls.ca_cert_path = config.getString(prefix + "ca_cert_path", "");
    tls.server_cert_path = config.getString(prefix + "server_cert_path", "");
    tls.server_key_path = config.getString(prefix + "server_key_path", "");
    tls.client_cert_path = config.getString(prefix + "client_cert_path", "");
    tls.client_key_path = config.getString(prefix + "client_key_path", "");
    tls.target_name_override = config.getString(prefix + "target_name_override", "");
    return tls;
}

bool GrpcTlsCredentials::validateForServer(const GrpcTlsConfig& config, std::string* error) {
    if (!config.enabled) {
        return true;
    }
    std::string content;
    if (!readFile(config.server_cert_path, &content, error)) {
        if (error && error->empty()) *error = "grpc tls server certificate is not readable";
        return false;
    }
    if (!readFile(config.server_key_path, &content, error)) {
        if (error && error->empty()) *error = "grpc tls server private key is not readable";
        return false;
    }
    if (config.require_client_auth && !readFile(config.ca_cert_path, &content, error)) {
        if (error && error->empty()) *error = "grpc tls CA certificate is required for client auth";
        return false;
    }
    return true;
}

bool GrpcTlsCredentials::validateForClient(const GrpcTlsConfig& config, std::string* error) {
    if (!config.enabled) {
        return true;
    }
    std::string content;
    if (!config.ca_cert_path.empty() && !readFile(config.ca_cert_path, &content, error)) {
        return false;
    }
    const bool has_client_cert = !config.client_cert_path.empty() || !config.client_key_path.empty();
    if (has_client_cert) {
        if (!readFile(config.client_cert_path, &content, error)) {
            if (error && error->empty()) *error = "grpc tls client certificate is not readable";
            return false;
        }
        if (!readFile(config.client_key_path, &content, error)) {
            if (error && error->empty()) *error = "grpc tls client private key is not readable";
            return false;
        }
    }
    return true;
}

bool GrpcTlsCredentials::readFile(const std::string& path, std::string* content, std::string* error) {
    if (path.empty()) {
        if (error) *error = "empty certificate path";
        return false;
    }
    std::ifstream input(path, std::ios::in | std::ios::binary);
    if (!input.is_open()) {
        if (error) *error = "failed to open certificate file: " + path;
        return false;
    }
    std::ostringstream ss;
    ss << input.rdbuf();
    if (content) *content = ss.str();
    return true;
}

#if defined(NEBULA_ENABLE_GRPC)
std::shared_ptr<grpc::ServerCredentials> GrpcTlsCredentials::serverCredentials(const GrpcTlsConfig& config) {
    if (!config.enabled) {
        return grpc::InsecureServerCredentials();
    }

    std::string error;
    if (!validateForServer(config, &error)) {
        LOG_ERROR("invalid gRPC TLS server config: " + error);
        return nullptr;
    }

    std::string cert_chain;
    std::string private_key;
    std::string root_certs;
    readFile(config.server_cert_path, &cert_chain, nullptr);
    readFile(config.server_key_path, &private_key, nullptr);
    if (!config.ca_cert_path.empty()) {
        readFile(config.ca_cert_path, &root_certs, nullptr);
    }

    grpc::SslServerCredentialsOptions options;
    grpc::SslServerCredentialsOptions::PemKeyCertPair key_cert_pair;
    key_cert_pair.private_key = private_key;
    key_cert_pair.cert_chain = cert_chain;
    options.pem_key_cert_pairs.push_back(key_cert_pair);
    options.pem_root_certs = root_certs;
    if (config.require_client_auth) {
        options.client_certificate_request = GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY;
    }
    return grpc::SslServerCredentials(options);
}

std::shared_ptr<grpc::ChannelCredentials> GrpcTlsCredentials::channelCredentials(const GrpcTlsConfig& config) {
    if (!config.enabled) {
        return grpc::InsecureChannelCredentials();
    }

    std::string error;
    if (!validateForClient(config, &error)) {
        LOG_ERROR("invalid gRPC TLS client config: " + error);
        return nullptr;
    }

    grpc::SslCredentialsOptions options;
    if (!config.ca_cert_path.empty()) {
        readFile(config.ca_cert_path, &options.pem_root_certs, nullptr);
    }
    if (!config.client_cert_path.empty()) {
        readFile(config.client_cert_path, &options.pem_cert_chain, nullptr);
    }
    if (!config.client_key_path.empty()) {
        readFile(config.client_key_path, &options.pem_private_key, nullptr);
    }
    return grpc::SslCredentials(options);
}

std::shared_ptr<grpc::Channel> GrpcTlsCredentials::createChannel(const std::string& address, const GrpcTlsConfig& config) {
    auto credentials = channelCredentials(config);
    if (!credentials) {
        return nullptr;
    }
    grpc::ChannelArguments args;
    if (config.enabled && !config.target_name_override.empty()) {
        args.SetSslTargetNameOverride(config.target_name_override);
    }
    return grpc::CreateCustomChannel(address, credentials, args);
}
#endif

}  // namespace nebula
