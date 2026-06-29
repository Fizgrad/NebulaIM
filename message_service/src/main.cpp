#include "MessageServiceContext.h"
#include "MessageServiceImpl.h"

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
    nebula::MessageServiceContext context;
    if (!context.init(config_path)) {
        LOG_ERROR("failed to init MessageServiceContext");
        return 1;
    }

    nebula::MessageServiceImpl service(context.userDao(), context.groupDao(), context.messageDao(),
                                       context.offlineMessageDao(), context.redisClient(), context.kafkaProducer(),
                                       context.messageIdGenerator(), context.messageDeduplicator(), context.options());

    grpc::ServerBuilder builder;
    builder.AddListeningPort(context.listenAddress(), grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    if (!server) {
        LOG_ERROR("failed to start MessageService on " + context.listenAddress());
        return 1;
    }

    LOG_INFO("MessageService listening on " + context.listenAddress());
    server->Wait();
    return 0;
#else
    std::string config_path = argc > 1 ? argv[1] : "../config/nebula.conf";
    std::cout << "nebula_message_service placeholder ready, config=" << config_path << std::endl;
    std::cout << "gRPC/storage disabled: install dependencies to enable MessageService" << std::endl;
    return 0;
#endif
}
