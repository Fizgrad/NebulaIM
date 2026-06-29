#include "common/rpc/GrpcClient.h"
#include "common/rpc/GrpcTlsCredentials.h"

#if defined(NEBULA_ENABLE_GRPC)
#include <grpcpp/grpcpp.h>
#endif

namespace nebula {

std::shared_ptr<grpc::Channel> GrpcClient::createInsecureChannel(const std::string& address) {
#if defined(NEBULA_ENABLE_GRPC)
    return grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
#else
    (void)address;
    return nullptr;
#endif
}

std::shared_ptr<grpc::Channel> GrpcClient::createChannel(const std::string& address, const GrpcTlsConfig& tls_config) {
#if defined(NEBULA_ENABLE_GRPC)
    return GrpcTlsCredentials::createChannel(address, tls_config);
#else
    (void)address;
    (void)tls_config;
    return nullptr;
#endif
}

}  // namespace nebula
