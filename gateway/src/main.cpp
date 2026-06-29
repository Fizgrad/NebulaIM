#include "GatewayContext.h"
#include "GatewayServer.h"
#include "GatewayServiceImpl.h"

#include "common/log/Logger.h"
#include "common/net/EventLoop.h"
#include "common/net/InetAddress.h"
#include "common/rpc/GrpcTlsCredentials.h"

#if defined(NEBULA_ENABLE_GRPC)
#include <grpcpp/grpcpp.h>
#include <memory>
#include <thread>
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
#if defined(NEBULA_ENABLE_GRPC) && defined(NEBULA_ENABLE_STORAGE) && defined(__linux__)
    nebula::GatewayContext context;
    if (!context.init(parseConfigPath(argc, argv))) {
        LOG_ERROR("failed to init GatewayContext");
        return 1;
    }
    nebula::GatewayOptions options = context.options();
    nebula::EventLoop loop;
    nebula::InetAddress listen_addr(static_cast<uint16_t>(options.tcp_port), options.tcp_host);
    nebula::GatewayServer tcp_server(&loop, listen_addr, options.gateway_id, context.connectionManager(), context.onlineManager(), context.router(), options.tcp_worker_threads, options.heartbeat_timeout_ms, context.tcpTlsContext());
    tcp_server.start();

    nebula::GatewayServiceImpl rpc_service(context.connectionManager(), context.codec(), options.gateway_id);
    auto credentials = nebula::GrpcTlsCredentials::serverCredentials(context.grpcTlsConfig());
    if (!credentials) {
        LOG_ERROR("failed to initialize Gateway RPC credentials");
        return 1;
    }
    grpc::ServerBuilder builder;
    builder.AddListeningPort(context.rpcListenAddress(), credentials);
    builder.RegisterService(&rpc_service);
    std::unique_ptr<grpc::Server> rpc_server(builder.BuildAndStart());
    if (!rpc_server) {
        LOG_ERROR("failed to start Gateway RPC on " + context.rpcListenAddress());
        return 1;
    }
    std::thread rpc_thread([&]() { rpc_server->Wait(); });
    LOG_INFO(std::string("Gateway TCP listening on ") + options.tcp_host + ":" + std::to_string(options.tcp_port) +
             (options.tcp_tls.enabled ? " with native TLS" : " without native TLS"));
    LOG_INFO("Gateway RPC listening on " + context.rpcListenAddress());
    loop.loop();
    rpc_server->Shutdown();
    rpc_thread.join();
    return 0;
#else
    std::string config_path = argc > 1 ? argv[1] : "../config/nebula.conf";
    std::cout << "nebula_gateway not started, config=" << config_path << std::endl;
    std::cout << "Gateway TCP/gRPC requires Linux, gRPC, and storage dependencies" << std::endl;
    return 0;
#endif
}
