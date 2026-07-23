#include "TestDeps.h"
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
    std::string reason;
    if (!nebula::tests::initMySqlPool(&pool, 2, &reason)) return nebula::tests::skip("test_relation_dao_extra", reason);
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
