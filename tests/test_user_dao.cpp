#include "TestDeps.h"
#include "common/config/Config.h"
#include "common/dao/UserDao.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/utils/TimeUtil.h"

#include <cassert>

int main() {
    nebula::MySqlConnectionPool pool;
    std::string reason;
    if (!nebula::tests::initMySqlPool(&pool, 2, &reason)) return nebula::tests::skip("test_user_dao", reason);

    int64_t now = nebula::TimeUtil::nowMs();
    nebula::User user;
    user.username = "test_user_" + std::to_string(now);
    user.password_hash = "hash";
    user.nickname = "nick";
    user.avatar = "avatar";
    user.created_at = now;
    user.updated_at = now;

    nebula::UserDao dao(pool);
    uint64_t user_id = 0;
    assert(dao.createUser(user, &user_id));
    assert(user_id > 0);

    auto by_id = dao.getUserById(user_id);
    assert(by_id.has_value());
    assert(by_id->username == user.username);

    auto by_name = dao.getUserByUsername(user.username);
    assert(by_name.has_value());
    assert(by_name->id == user_id);

    assert(dao.updateUserProfile(user_id, "new_nick", "new_avatar"));
    auto updated = dao.getUserById(user_id);
    assert(updated.has_value());
    assert(updated->nickname == "new_nick");
    assert(updated->avatar == "new_avatar");

    return 0;
}
