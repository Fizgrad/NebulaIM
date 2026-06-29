#pragma once

#include "common/auth/TokenManager.h"
#include "common/config/Config.h"
#include "common/dao/UserDao.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/redis/RedisClient.h"

#include <memory>
#include <string>

namespace nebula {

class UserServiceContext {
public:
    UserServiceContext();
    ~UserServiceContext();

    bool init(const std::string& config_path);

    UserDao* userDao();
    RedisClient* redisClient();
    TokenManager* tokenManager();

    std::string listenAddress() const;
    int passwordMinLength() const;

private:
    Config config_;
    MySqlConnectionPool mysql_pool_;
    std::unique_ptr<UserDao> user_dao_;
    std::unique_ptr<RedisClient> redis_client_;
    std::unique_ptr<TokenManager> token_manager_;

    std::string listen_address_;
    int password_min_length_;
};

}  // namespace nebula
