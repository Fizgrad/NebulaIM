#include "common/rpc/GrpcServer.h"

#if defined(NEBULA_ENABLE_GRPC)
#include "common/log/Logger.h"

#include <grpcpp/grpcpp.h>
#endif

namespace nebula {

GrpcServer::GrpcServer(std::string address)
    : address_(std::move(address)),
      service_(nullptr) {}

GrpcServer::~GrpcServer() {
    shutdown();
}

void GrpcServer::registerService(grpc::Service* service) {
    service_ = service;
}

bool GrpcServer::start() {
#if defined(NEBULA_ENABLE_GRPC)
    if (service_ == nullptr) {
        return false;
    }
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address_, grpc::InsecureServerCredentials());
    builder.RegisterService(service_);
    server_ = builder.BuildAndStart();
    if (!server_) {
        return false;
    }
    LOG_INFO("gRPC server listening on " + address_);
    return true;
#else
    return false;
#endif
}

void GrpcServer::wait() {
#if defined(NEBULA_ENABLE_GRPC)
    if (server_) {
        server_->Wait();
    }
#endif
}

void GrpcServer::shutdown() {
#if defined(NEBULA_ENABLE_GRPC)
    if (server_) {
        server_->Shutdown();
    }
#endif
}

}  // namespace nebula
