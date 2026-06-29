#include "common/rpc/GrpcClient.h"

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

}  // namespace nebula
