#pragma once

#include "common/config/Config.h"
#include "common/async/RpcExecutor.h"
#include "common/discovery/StaticServiceResolver.h"
#include "common/gateway/ConnectionManager.h"
#include "common/gateway/GatewayBackendClients.h"
#include "common/gateway/GatewayOnlineManager.h"
#include "common/gateway/GatewayRouter.h"
#include "common/protocol/PacketCodec.h"
#include "common/redis/RedisClient.h"
#include "common/rpc/GrpcTlsCredentials.h"
#include "common/tls/TlsConfig.h"
#include "common/tls/TlsContext.h"

#include <memory>
#include <string>

namespace nebula {

struct GatewayOptions {
    std::string gateway_id = "gateway-1";
    std::string tcp_host = "0.0.0.0";
    int tcp_port = 9000;
    int tcp_worker_threads = 4;
    int rpc_worker_threads = 4;
    int rpc_max_queue_size = 10000;
    std::string rpc_host = "0.0.0.0";
    int rpc_port = 50055;
    int heartbeat_timeout_ms = 30000;
    int online_ttl_seconds = 60;
    TlsConfig tcp_tls;
    std::string user_service_addr = "127.0.0.1:50051";
    std::string message_service_addr = "127.0.0.1:50052";
    std::string relation_service_addr = "127.0.0.1:50053";
    std::string push_service_addr = "127.0.0.1:50054";
};

class GatewayContext {
public:
    GatewayContext();
    ~GatewayContext();

    bool init(const std::string& config_path);

    GatewayOptions options() const;
    ConnectionManager* connectionManager();
    GatewayOnlineManager* onlineManager();
    GatewayBackendClients* backendClients();
    GatewayRouter* router();
    RpcExecutor* rpcExecutor();
    RedisClient* redisClient();
    PacketCodec* codec();

    std::string tcpListenIp() const;
    int tcpListenPort() const;
    std::string rpcListenAddress() const;
    const GrpcTlsConfig& grpcTlsConfig() const;
    std::shared_ptr<TlsContext> tcpTlsContext() const;

private:
    Config config_;
    GatewayOptions options_;
    std::unique_ptr<RedisClient> redis_client_;
    std::unique_ptr<ConnectionManager> connection_manager_;
    std::unique_ptr<GatewayOnlineManager> online_manager_;
    std::unique_ptr<GatewayBackendClients> backend_clients_;
    std::unique_ptr<StaticServiceResolver> service_resolver_;
    std::unique_ptr<GatewayRouter> router_;
    std::unique_ptr<PacketCodec> codec_;
    std::unique_ptr<RpcExecutor> rpc_executor_;
    GrpcTlsConfig grpc_tls_config_;
    std::shared_ptr<TlsContext> tcp_tls_context_;
};

}  // namespace nebula
