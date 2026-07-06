#include "GatewayContext.h"

#include "common/log/Logger.h"
#include "common/rpc/InternalRpcAuth.h"
#include "common/trace/TraceManager.h"

namespace nebula {

GatewayContext::GatewayContext() = default;
GatewayContext::~GatewayContext() = default;

bool GatewayContext::init(const std::string& config_path) {
    if (!config_.loadFromFile(config_path)) return false;
    InternalRpcAuth::instance().configureFromConfig(config_);
    options_.gateway_id = config_.getString("gateway.id", options_.gateway_id);
    options_.tcp_host = config_.getString("gateway.tcp.host", options_.tcp_host);
    options_.tcp_port = config_.getInt("gateway.tcp.port", options_.tcp_port);
    options_.tcp_worker_threads = config_.getInt("gateway.tcp.worker_threads", options_.tcp_worker_threads);
    options_.rpc_worker_threads = config_.getInt("gateway.rpc_worker_threads", options_.rpc_worker_threads);
    options_.rpc_max_queue_size = config_.getInt("gateway.rpc_max_queue_size", options_.rpc_max_queue_size);
    options_.rpc_host = config_.getString("gateway.rpc.host", options_.rpc_host);
    options_.rpc_port = config_.getInt("gateway.rpc.port", options_.rpc_port);
    options_.heartbeat_timeout_ms = config_.getInt("gateway.heartbeat_timeout_ms", options_.heartbeat_timeout_ms);
    options_.online_ttl_seconds = config_.getInt("gateway.online_ttl_seconds", options_.online_ttl_seconds);
    options_.tcp_tls = TlsConfigLoader::fromConfig(config_, "gateway.tls.");
    options_.user_service_addr = config_.getString("user_service.addr", options_.user_service_addr);
    options_.message_service_addr = config_.getString("message_service.addr", options_.message_service_addr);
    options_.relation_service_addr = config_.getString("relation_service.addr", options_.relation_service_addr);
    options_.push_service_addr = config_.getString("push_service.addr", options_.push_service_addr);
    grpc_tls_config_ = GrpcTlsCredentials::fromConfig(config_);
    TraceManager::instance().configure(TraceManager::configFrom(config_, "nebula-gateway"));

    if (options_.tcp_tls.enabled) {
        std::string tls_error;
        tcp_tls_context_ = TlsContext::createServer(options_.tcp_tls, &tls_error);
        if (!tcp_tls_context_) {
            LOG_ERROR("failed to initialize Gateway TCP TLS: " + tls_error);
            return false;
        }
    }

    auto redis = std::make_unique<RedisClient>();
    RedisConfig redis_config;
    redis_config.host = config_.getString("redis.host", redis_config.host);
    redis_config.port = config_.getInt("redis.port", redis_config.port);
    redis_config.timeout_ms = config_.getInt("redis.timeout_ms", redis_config.timeout_ms);
    redis_config.password = config_.getString("redis.password", redis_config.password);
    if (!redis->connect(redis_config)) return false;

    redis_client_ = std::move(redis);
    connection_manager_ = std::make_unique<ConnectionManager>(options_.gateway_id);
    online_manager_ = std::make_unique<GatewayOnlineManager>(redis_client_.get(), options_.gateway_id, options_.online_ttl_seconds);
    backend_clients_ = std::make_unique<GatewayBackendClients>();
    service_resolver_ = std::make_unique<StaticServiceResolver>();
    service_resolver_->loadFromConfig(config_);
    if (!backend_clients_->init(*service_resolver_, grpc_tls_config_)) return false;
    codec_ = std::make_unique<PacketCodec>();
    rpc_executor_ = std::make_unique<RpcExecutor>(static_cast<size_t>(options_.rpc_worker_threads),
                                                  static_cast<size_t>(options_.rpc_max_queue_size));
    rpc_executor_->start();
    router_ = std::make_unique<GatewayRouter>(connection_manager_.get(), online_manager_.get(), backend_clients_.get(), codec_.get(), nullptr, rpc_executor_.get());
    return true;
}

GatewayOptions GatewayContext::options() const { return options_; }
ConnectionManager* GatewayContext::connectionManager() { return connection_manager_.get(); }
GatewayOnlineManager* GatewayContext::onlineManager() { return online_manager_.get(); }
GatewayBackendClients* GatewayContext::backendClients() { return backend_clients_.get(); }
GatewayRouter* GatewayContext::router() { return router_.get(); }
RpcExecutor* GatewayContext::rpcExecutor() { return rpc_executor_.get(); }
RedisClient* GatewayContext::redisClient() { return redis_client_.get(); }
PacketCodec* GatewayContext::codec() { return codec_.get(); }
std::string GatewayContext::tcpListenIp() const { return options_.tcp_host; }
int GatewayContext::tcpListenPort() const { return options_.tcp_port; }
std::string GatewayContext::rpcListenAddress() const { return options_.rpc_host + ":" + std::to_string(options_.rpc_port); }
const GrpcTlsConfig& GatewayContext::grpcTlsConfig() const { return grpc_tls_config_; }
std::shared_ptr<TlsContext> GatewayContext::tcpTlsContext() const { return tcp_tls_context_; }

}  // namespace nebula
