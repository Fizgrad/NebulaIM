#include "AdminServiceContext.h"
#include "AdminServiceImpl.h"

#include "common/app/ShutdownSignal.h"
#include "common/log/Logger.h"
#include "common/monitor/MetricsRuntime.h"
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
#if defined(NEBULA_ENABLE_GRPC)
    std::string config_path = parseConfigPath(argc, argv);
    nebula::AdminServiceContext context;
    if (!context.init(config_path)) {
        LOG_ERROR("failed to init AdminServiceContext");
        return 1;
    }
#if defined(NEBULA_ENABLE_STORAGE)
    nebula::AdminServiceImpl service(context.adminAuth(),
                                     context.runtimeConfig(),
                                     context.mysqlPool(),
                                     context.redisClient(),
                                     context.cleanupOptions(),
                                     context.kafkaConsumerConfig(),
                                     context.kafkaTopics());
#else
    nebula::AdminServiceImpl service(context.adminAuth());
#endif
    auto credentials = nebula::GrpcTlsCredentials::serverCredentials(context.grpcTlsConfig());
    if (!credentials) {
        LOG_ERROR("failed to initialize AdminService gRPC credentials");
        return 1;
    }
    grpc::ServerBuilder builder;
    builder.AddListeningPort(context.listenAddress(), credentials);
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    if (!server) {
        LOG_ERROR("failed to start AdminService on " + context.listenAddress());
        return 1;
    }
    LOG_INFO("AdminService listening on " + context.listenAddress());
    auto metrics_server = nebula::startMetricsServerFromConfig(config_path, "admin_service", 9106);
    nebula::ShutdownSignalWatcher shutdown([&server]() {
        if (server) server->Shutdown();
    });
    server->Wait();
    return 0;
#else
    std::string config_path = argc > 1 ? argv[1] : "../config/nebula.conf";
    std::cout << "nebula_admin_service not started, config=" << config_path << std::endl;
    std::cout << "gRPC disabled: install dependencies to enable AdminService" << std::endl;
    return 0;
#endif
}
