#pragma once

#include "common/config/Config.h"

#include <memory>
#include <string>

#if defined(NEBULA_ENABLE_GRPC)
#include <grpcpp/grpcpp.h>
#endif

namespace nebula {

struct GrpcTlsConfig {
    bool enabled = false;
    bool require_client_auth = false;
    std::string ca_cert_path;
    std::string server_cert_path;
    std::string server_key_path;
    std::string client_cert_path;
    std::string client_key_path;
    std::string target_name_override;
};

class GrpcTlsCredentials {
public:
    static GrpcTlsConfig fromConfig(const Config& config, const std::string& prefix = "grpc.tls.");
    static bool validateForServer(const GrpcTlsConfig& config, std::string* error = nullptr);
    static bool validateForClient(const GrpcTlsConfig& config, std::string* error = nullptr);

#if defined(NEBULA_ENABLE_GRPC)
    static std::shared_ptr<grpc::ServerCredentials> serverCredentials(const GrpcTlsConfig& config);
    static std::shared_ptr<grpc::ChannelCredentials> channelCredentials(const GrpcTlsConfig& config);
    static std::shared_ptr<grpc::Channel> createChannel(const std::string& address, const GrpcTlsConfig& config);
#endif

private:
    static bool readFile(const std::string& path, std::string* content, std::string* error);
};

}  // namespace nebula
