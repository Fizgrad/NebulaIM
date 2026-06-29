#pragma once

#include <memory>
#include <string>

namespace nebula {
struct GrpcTlsConfig;
}  // namespace nebula

#if defined(NEBULA_ENABLE_GRPC)
#include <grpcpp/grpcpp.h>
#else
namespace grpc {
class Channel;
}  // namespace grpc
#endif

namespace nebula {

class GrpcClient {
public:
    static std::shared_ptr<grpc::Channel> createInsecureChannel(const std::string& address);
    static std::shared_ptr<grpc::Channel> createChannel(const std::string& address, const GrpcTlsConfig& tls_config);
};

}  // namespace nebula
