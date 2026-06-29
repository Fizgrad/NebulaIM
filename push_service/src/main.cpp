#include "PushServiceContext.h"
#include "PushServiceImpl.h"

#include "common/log/Logger.h"
#include "common/rpc/GrpcTlsCredentials.h"

#if defined(NEBULA_ENABLE_GRPC)
#include <grpcpp/grpcpp.h>
#include <memory>
#endif

#include <iostream>
#include <string>

namespace {
std::string parseConfigPath(int argc, char** argv) {
    std::string config_path = "../config/nebula.conf";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) config_path = argv[++i];
        else config_path = arg;
    }
    return config_path;
}
}

int main(int argc, char* argv[]) {
#if defined(NEBULA_ENABLE_GRPC) && defined(NEBULA_ENABLE_STORAGE)
    nebula::PushServiceContext context;
    if (!context.init(parseConfigPath(argc, argv))) {
        LOG_ERROR("failed to init PushServiceContext");
        return 1;
    }
    if (!context.startWorkers()) {
        LOG_ERROR("failed to start PushWorker");
        return 1;
    }
    nebula::PushServiceImpl service(context.dispatcher());
    auto credentials = nebula::GrpcTlsCredentials::serverCredentials(context.grpcTlsConfig());
    if (!credentials) {
        LOG_ERROR("failed to initialize PushService gRPC credentials");
        context.stopWorkers();
        return 1;
    }
    grpc::ServerBuilder builder;
    builder.AddListeningPort(context.listenAddress(), credentials);
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    if (!server) {
        LOG_ERROR("failed to start PushService on " + context.listenAddress());
        context.stopWorkers();
        return 1;
    }
    LOG_INFO("PushService listening on " + context.listenAddress());
    server->Wait();
    context.stopWorkers();
    return 0;
#else
    std::string config_path = argc > 1 ? argv[1] : "../config/nebula.conf";
    std::cout << "nebula_push_service placeholder ready, config=" << config_path << std::endl;
    std::cout << "gRPC/storage disabled: install dependencies to enable PushService" << std::endl;
    return 0;
#endif
}
