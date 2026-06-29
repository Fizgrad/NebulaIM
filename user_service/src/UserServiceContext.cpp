#include "UserServiceContext.h"

#include "common/log/Logger.h"
#include "common/trace/TraceManager.h"

namespace nebula {

UserServiceContext::UserServiceContext()
    : password_min_length_(6) {}

UserServiceContext::~UserServiceContext() = default;

bool UserServiceContext::init(const std::string& config_path) {
    if (!config_.loadFromFile(config_path)) {
        LOG_ERROR("failed to load config: " + config_path);
        return false;
    }
    TraceManager::instance().configure(TraceManager::configFrom(config_, "nebula-user-service"));

    MySqlConfig mysql;
    mysql.host = config_.getString("mysql.host", mysql.host);
    mysql.port = config_.getInt("mysql.port", mysql.port);
    mysql.user = config_.getString("mysql.user", mysql.user);
    mysql.password = config_.getString("mysql.password", mysql.password);
    mysql.database = config_.getString("mysql.database", mysql.database);
    int pool_size = config_.getInt("mysql.pool_size", 4);
    if (!mysql_pool_.init(mysql, static_cast<size_t>(pool_size))) {
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

    std::string host = config_.getString("user_service.host", "0.0.0.0");
    int port = config_.getInt("user_service.port", 50051);
    listen_address_ = host + ":" + std::to_string(port);
    grpc_tls_config_ = GrpcTlsCredentials::fromConfig(config_);
    password_min_length_ = config_.getInt("auth.password_min_length", 6);

    user_dao_ = std::make_unique<UserDao>(mysql_pool_);
    redis_client_ = std::move(redis);
    token_manager_ = std::make_unique<TokenManager>(config_.getInt("auth.token_ttl_seconds", 7 * 24 * 3600));
    return true;
}

UserDao* UserServiceContext::userDao() {
    return user_dao_.get();
}

RedisClient* UserServiceContext::redisClient() {
    return redis_client_.get();
}

TokenManager* UserServiceContext::tokenManager() {
    return token_manager_.get();
}

std::string UserServiceContext::listenAddress() const {
    return listen_address_;
}

const GrpcTlsConfig& UserServiceContext::grpcTlsConfig() const {
    return grpc_tls_config_;
}

int UserServiceContext::passwordMinLength() const {
    return password_min_length_;
}

}  // namespace nebula
