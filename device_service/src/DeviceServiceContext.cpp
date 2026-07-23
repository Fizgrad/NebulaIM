#include "DeviceServiceContext.h"

#include "common/log/Logger.h"
#include "common/rpc/InternalRpcAuth.h"
#include "common/trace/TraceManager.h"

namespace nebula {

DeviceServiceContext::DeviceServiceContext() = default;
DeviceServiceContext::~DeviceServiceContext() = default;

bool DeviceServiceContext::init(const std::string& config_path) {
    if (!config_.loadFromFile(config_path)) {
        LOG_ERROR("failed to load config: " + config_path);
        return false;
    }
    InternalRpcAuth::instance().configureFromConfig(config_);
    TraceManager::instance().configure(TraceManager::configFrom(config_, "nebula-device-service"));

    MySqlConfig mysql;
    mysql.host = config_.getString("mysql.host", mysql.host);
    mysql.port = config_.getInt("mysql.port", mysql.port);
    mysql.user = config_.getString("mysql.user", mysql.user);
    mysql.password = config_.getString("mysql.password", mysql.password);
    mysql.database = config_.getString("mysql.database", mysql.database);
    if (!mysql_pool_.init(mysql, static_cast<size_t>(config_.getInt("mysql.pool_size", 4)))) {
        LOG_ERROR("failed to initialize MySQL pool");
        return false;
    }

    auto redis = std::make_unique<RedisClient>();
    RedisConfig redis_config;
    redis_config.host = config_.getString("redis.host", redis_config.host);
    redis_config.port = config_.getInt("redis.port", redis_config.port);
    redis_config.timeout_ms = config_.getInt("redis.timeout_ms", redis_config.timeout_ms);
    redis_config.password = config_.getString("redis.password", redis_config.password);
    if (!redis->connect(redis_config)) {
        LOG_ERROR("failed to connect Redis: " + redis->lastError());
        return false;
    }

    grpc_tls_config_ = GrpcTlsCredentials::fromConfig(config_);
    service_resolver_ = std::make_unique<StaticServiceResolver>();
    service_resolver_->loadFromConfig(config_);
    gateway_client_manager_ = std::make_unique<GatewayClientManager>();
    if (!gateway_client_manager_->initFromResolver(*service_resolver_, grpc_tls_config_)) {
        LOG_ERROR("failed to initialize Gateway clients");
        return false;
    }

    user_dao_ = std::make_unique<UserDao>(mysql_pool_);
    device_dao_ = std::make_unique<DeviceDao>(mysql_pool_);
    redis_client_ = std::move(redis);

    std::string host = config_.getString("device_service.host", "0.0.0.0");
    int port = config_.getInt("device_service.port", 50058);
    listen_address_ = host + ":" + std::to_string(port);
    return true;
}

UserDao* DeviceServiceContext::userDao() { return user_dao_.get(); }
DeviceDao* DeviceServiceContext::deviceDao() { return device_dao_.get(); }
RedisClient* DeviceServiceContext::redisClient() { return redis_client_.get(); }
GatewayClientManager* DeviceServiceContext::gatewayClientManager() { return gateway_client_manager_.get(); }
std::string DeviceServiceContext::listenAddress() const { return listen_address_; }
const GrpcTlsConfig& DeviceServiceContext::grpcTlsConfig() const { return grpc_tls_config_; }

}  // namespace nebula
