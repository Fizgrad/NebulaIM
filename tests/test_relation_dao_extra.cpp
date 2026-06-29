#include "common/auth/PasswordHasher.h"
#include "common/config/Config.h"
#include "common/dao/GroupDao.h"
#include "common/dao/RelationDao.h"
#include "common/dao/UserDao.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/utils/TimeUtil.h"

#include <algorithm>
#include <cassert>

namespace {
nebula::MySqlConfig mysqlConfig() {
    nebula::Config config;
    assert(config.loadFromFile("config/nebula.conf"));
    nebula::MySqlConfig mysql;
    mysql.host = config.getString("mysql.host", mysql.host);
    mysql.port = config.getInt("mysql.port", mysql.port);
    mysql.user = config.getString("mysql.user", mysql.user);
    mysql.password = config.getString("mysql.password", mysql.password);
    mysql.database = config.getString("mysql.database", mysql.database);
    return mysql;
}

nebula::User makeUser(const std::string& prefix) {
    int64_t now = nebula::TimeUtil::nowMs();
    nebula::User user;
    user.username = prefix + std::to_string(now);
    user.password_hash = nebula::PasswordHasher::hashPassword("password123");
    user.nickname = prefix;
    user.created_at = now;
    user.updated_at = now;
    return user;
}
}

int main() {
    nebula::MySqlConnectionPool pool;
    assert(pool.init(mysqlConfig(), 2));
    nebula::UserDao user_dao(pool);
    nebula::RelationDao relation_dao(pool);

    uint64_t u1 = 0, u2 = 0;
    assert(user_dao.createUser(makeUser("rel_a_"), &u1));
    assert(user_dao.createUser(makeUser("rel_b_"), &u2));

    assert(relation_dao.addFriendBidirectional(u1, u2));
    assert(relation_dao.isFriend(u1, u2));
    assert(relation_dao.isFriend(u2, u1));
    auto f1 = relation_dao.listFriends(u1);
    auto f2 = relation_dao.listFriends(u2);
    assert(std::find(f1.begin(), f1.end(), u2) != f1.end());
    assert(std::find(f2.begin(), f2.end(), u1) != f2.end());
    assert(relation_dao.addFriendBidirectional(u1, u2));
    assert(relation_dao.deleteFriendBidirectional(u1, u2));
    assert(!relation_dao.isFriend(u1, u2));
    assert(!relation_dao.isFriend(u2, u1));
    assert(relation_dao.deleteFriendBidirectional(u1, u2));
    return 0;
}
