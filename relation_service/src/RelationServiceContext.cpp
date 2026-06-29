#include "RelationServiceContext.h"

#include "common/log/Logger.h"
#include "common/trace/TraceManager.h"

namespace nebula {

RelationServiceContext::RelationServiceContext() = default;
RelationServiceContext::~RelationServiceContext() = default;

bool RelationServiceContext::init(const std::string& config_path) {
    if (!config_.loadFromFile(config_path)) {
        LOG_ERROR("failed to load config: " + config_path);
        return false;
    }
    TraceManager::instance().configure(TraceManager::configFrom(config_, "nebula-relation-service"));

    MySqlConfig mysql;
    mysql.host = config_.getString("mysql.host", mysql.host);
    mysql.port = config_.getInt("mysql.port", mysql.port);
    mysql.user = config_.getString("mysql.user", mysql.user);
    mysql.password = config_.getString("mysql.password", mysql.password);
    mysql.database = config_.getString("mysql.database", mysql.database);
    int pool_size = config_.getInt("mysql.pool_size", 4);
    if (!mysql_pool_.init(mysql, static_cast<size_t>(pool_size))) {
        LOG_ERROR("failed to initialize MySQL pool for RelationService");
        return false;
    }

    std::string host = config_.getString("relation_service.host", "0.0.0.0");
    int port = config_.getInt("relation_service.port", 50053);
    listen_address_ = host + ":" + std::to_string(port);
    grpc_tls_config_ = GrpcTlsCredentials::fromConfig(config_);

    user_dao_ = std::make_unique<UserDao>(mysql_pool_);
    relation_dao_ = std::make_unique<RelationDao>(mysql_pool_);
    group_dao_ = std::make_unique<GroupDao>(mysql_pool_);
    friend_request_dao_ = std::make_unique<FriendRequestDao>(mysql_pool_);
    return true;
}

UserDao* RelationServiceContext::userDao() {
    return user_dao_.get();
}

RelationDao* RelationServiceContext::relationDao() {
    return relation_dao_.get();
}

GroupDao* RelationServiceContext::groupDao() {
    return group_dao_.get();
}

FriendRequestDao* RelationServiceContext::friendRequestDao() {
    return friend_request_dao_.get();
}

std::string RelationServiceContext::listenAddress() const {
    return listen_address_;
}

const GrpcTlsConfig& RelationServiceContext::grpcTlsConfig() const {
    return grpc_tls_config_;
}

}  // namespace nebula
