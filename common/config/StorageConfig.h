#pragma once

#include "common/config/Config.h"
#include "common/db/MySqlConnection.h"
#include "common/redis/RedisClient.h"

namespace nebula {

MySqlConfig loadMySqlConfig(const Config& config);
RedisConfig loadRedisConfig(const Config& config);

}  // namespace nebula
