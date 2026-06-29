#pragma once

#include <memory>
#include <string>

#include "common/rpc/GrpcTlsCredentials.h"

#if defined(NEBULA_ENABLE_GRPC)
#include <grpcpp/grpcpp.h>
#else
namespace grpc {
class Server;
class Service;
}  // namespace grpc
#endif

namespace nebula {

class GrpcServer {
public:
    explicit GrpcServer(std::string address);
    GrpcServer(std::string address, GrpcTlsConfig tls_config);
    ~GrpcServer();

    GrpcServer(const GrpcServer&) = delete;
    GrpcServer& operator=(const GrpcServer&) = delete;

    void registerService(grpc::Service* service);
    bool start();
    void wait();
    void shutdown();

private:
    std::string address_;
    GrpcTlsConfig tls_config_;
    grpc::Service* service_;
    std::shared_ptr<grpc::Server> server_;
};

}  // namespace nebula
