#pragma once

#include "common/config/Config.h"
#include "common/conversation/ConversationDao.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/rpc/GrpcTlsCredentials.h"

#include <memory>
#include <string>

namespace nebula {

class ConversationServiceContext {
public:
    bool init(const std::string& config_path);

    ConversationDao* conversationDao();
    std::string listenAddress() const;
    const GrpcTlsConfig& grpcTlsConfig() const;

private:
    Config config_;
    MySqlConnectionPool mysql_pool_;
    std::unique_ptr<ConversationDao> conversation_dao_;
    std::string listen_address_;
    GrpcTlsConfig grpc_tls_config_;
};

}  // namespace nebula
