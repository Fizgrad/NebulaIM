#include "common/auth/PasswordHasher.h"
#include "common/config/Config.h"
#include "common/dao/GroupDao.h"
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
    nebula::GroupDao group_dao(pool);

    uint64_t owner = 0, member = 0;
    assert(user_dao.createUser(makeUser("owner_"), &owner));
    assert(user_dao.createUser(makeUser("member_"), &member));

    int64_t now = nebula::TimeUtil::nowMs();
    nebula::Group group;
    group.group_name = "group_" + std::to_string(now);
    group.owner_id = owner;
    group.created_at = now;
    group.updated_at = now;
    uint64_t group_id = 0;
    assert(group_dao.createGroup(group, &group_id));
    assert(group_id > 0);
    assert(group_dao.getGroupById(group_id).has_value());
    assert(group_dao.isMember(group_id, owner));
    assert(group_dao.isOwner(group_id, owner));
    assert(group_dao.addMember(group_id, member, 0));
    assert(group_dao.isMember(group_id, member));
    auto members = group_dao.listMembers(group_id);
    assert(std::find(members.begin(), members.end(), owner) != members.end());
    assert(std::find(members.begin(), members.end(), member) != members.end());
    assert(group_dao.removeMember(group_id, member));
    assert(!group_dao.isMember(group_id, member));
    return 0;
}
