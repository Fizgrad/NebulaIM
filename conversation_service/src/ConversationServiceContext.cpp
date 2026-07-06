#include "ConversationServiceContext.h"

#include "common/log/Logger.h"
#include "common/rpc/InternalRpcAuth.h"
#include "common/trace/TraceManager.h"

namespace nebula {

bool ConversationServiceContext::init(const std::string& config_path) {
    if (!config_.loadFromFile(config_path)) return false;
    InternalRpcAuth::instance().configureFromConfig(config_);
    TraceManager::instance().configure(TraceManager::configFrom(config_, "nebula-conversation-service"));
    MySqlConfig mysql;
    mysql.host = config_.getString("mysql.host", mysql.host);
    mysql.port = config_.getInt("mysql.port", mysql.port);
    mysql.user = config_.getString("mysql.user", mysql.user);
    mysql.password = config_.getString("mysql.password", mysql.password);
    mysql.database = config_.getString("mysql.database", mysql.database);
    if (!mysql_pool_.init(mysql, static_cast<size_t>(config_.getInt("mysql.pool_size", 4)))) {
        LOG_ERROR("failed to initialize MySQL pool for ConversationService");
        return false;
    }
    std::string host = config_.getString("conversation_service.host", "0.0.0.0");
    int port = config_.getInt("conversation_service.port", 50056);
    listen_address_ = host + ":" + std::to_string(port);
    grpc_tls_config_ = GrpcTlsCredentials::fromConfig(config_);
    conversation_dao_ = std::make_unique<ConversationDao>(mysql_pool_);
    return true;
}

ConversationDao* ConversationServiceContext::conversationDao() {
    return conversation_dao_.get();
}

std::string ConversationServiceContext::listenAddress() const {
    return listen_address_;
}

const GrpcTlsConfig& ConversationServiceContext::grpcTlsConfig() const {
    return grpc_tls_config_;
}

}  // namespace nebula
