#pragma once

#include "common/config/Config.h"
#include "common/dao/FriendRequestDao.h"
#include "common/dao/GroupDao.h"
#include "common/dao/RelationDao.h"
#include "common/dao/UserDao.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/rpc/GrpcTlsCredentials.h"

#include <memory>
#include <string>

namespace nebula {

class RelationServiceContext {
public:
    RelationServiceContext();
    ~RelationServiceContext();

    bool init(const std::string& config_path);

    UserDao* userDao();
    RelationDao* relationDao();
    GroupDao* groupDao();
    FriendRequestDao* friendRequestDao();

    std::string listenAddress() const;
    const GrpcTlsConfig& grpcTlsConfig() const;

private:
    Config config_;
    MySqlConnectionPool mysql_pool_;

    std::unique_ptr<UserDao> user_dao_;
    std::unique_ptr<RelationDao> relation_dao_;
    std::unique_ptr<GroupDao> group_dao_;
    std::unique_ptr<FriendRequestDao> friend_request_dao_;

    std::string listen_address_;
    GrpcTlsConfig grpc_tls_config_;
};

}  // namespace nebula
