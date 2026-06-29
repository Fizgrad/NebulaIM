#pragma once

#include "common/config/Config.h"
#include "common/dao/GroupDao.h"
#include "common/dao/RelationDao.h"
#include "common/dao/UserDao.h"
#include "common/db/MySqlConnectionPool.h"

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

    std::string listenAddress() const;

private:
    Config config_;
    MySqlConnectionPool mysql_pool_;

    std::unique_ptr<UserDao> user_dao_;
    std::unique_ptr<RelationDao> relation_dao_;
    std::unique_ptr<GroupDao> group_dao_;

    std::string listen_address_;
};

}  // namespace nebula
