#pragma once

#include <memory>
#include <string>

namespace grpc {
class Server;
class Service;
}  // namespace grpc

namespace nebula {

class GrpcServer {
public:
    explicit GrpcServer(std::string address);
    ~GrpcServer();

    GrpcServer(const GrpcServer&) = delete;
    GrpcServer& operator=(const GrpcServer&) = delete;

    void registerService(grpc::Service* service);
    bool start();
    void wait();
    void shutdown();

private:
    std::string address_;
    grpc::Service* service_;
    std::shared_ptr<grpc::Server> server_;
};

}  // namespace nebula
