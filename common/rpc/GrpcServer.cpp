#include "common/rpc/GrpcServer.h"

#if defined(NEBULA_ENABLE_GRPC)
#include "common/log/Logger.h"

#include <grpcpp/grpcpp.h>
#endif

namespace nebula {

GrpcServer::GrpcServer(std::string address)
    : GrpcServer(std::move(address), GrpcTlsConfig{}) {}

GrpcServer::GrpcServer(std::string address, GrpcTlsConfig tls_config)
    : address_(std::move(address)),
      tls_config_(std::move(tls_config)),
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
    auto credentials = GrpcTlsCredentials::serverCredentials(tls_config_);
    if (!credentials) {
        return false;
    }
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address_, credentials);
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
