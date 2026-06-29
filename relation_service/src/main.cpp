#include "RelationServiceContext.h"
#include "RelationServiceImpl.h"

#include "common/log/Logger.h"

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
    std::string config_path = parseConfigPath(argc, argv);
    nebula::RelationServiceContext context;
    if (!context.init(config_path)) {
        LOG_ERROR("failed to init RelationServiceContext");
        return 1;
    }

    nebula::RelationServiceImpl service(context.userDao(), context.relationDao(), context.groupDao());

    grpc::ServerBuilder builder;
    builder.AddListeningPort(context.listenAddress(), grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    if (!server) {
        LOG_ERROR("failed to start RelationService on " + context.listenAddress());
        return 1;
    }

    LOG_INFO("RelationService listening on " + context.listenAddress());
    server->Wait();
    return 0;
#else
    std::string config_path = argc > 1 ? argv[1] : "../config/nebula.conf";
    std::cout << "nebula_relation_service placeholder ready, config=" << config_path << std::endl;
    std::cout << "gRPC/storage disabled: install dependencies to enable RelationService" << std::endl;
    return 0;
#endif
}
