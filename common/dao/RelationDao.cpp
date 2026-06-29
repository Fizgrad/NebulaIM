#include "common/dao/RelationDao.h"

#include "common/db/MySqlConnection.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/db/MySqlResult.h"
#include "common/db/MySqlTransaction.h"

namespace nebula {

RelationDao::RelationDao(MySqlConnectionPool& pool) : pool_(pool) {}

bool RelationDao::addFriend(uint64_t user_id, uint64_t friend_id) {
    if (user_id == 0 || friend_id == 0 || user_id == friend_id) return false;
    auto conn = pool_.acquire();
    if (!conn) return false;
    return addFriend(*conn, user_id, friend_id);
}

bool RelationDao::addFriend(MySqlConnection& conn, uint64_t user_id, uint64_t friend_id) {
    if (user_id == 0 || friend_id == 0 || user_id == friend_id) return false;
    std::string now = "UNIX_TIMESTAMP()*1000";
    std::string sql = "INSERT INTO friendships(user_id,friend_id,status,created_at,updated_at) VALUES(" +
                      std::to_string(user_id) + "," + std::to_string(friend_id) + ",1," + now + "," + now +
                      ") ON DUPLICATE KEY UPDATE status=1, updated_at=" + now;
    return conn.executeUpdate(sql);
}

bool RelationDao::deleteFriend(uint64_t user_id, uint64_t friend_id) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    return conn->executeUpdate("UPDATE friendships SET status=0, updated_at=UNIX_TIMESTAMP()*1000 WHERE user_id=" +
                               std::to_string(user_id) + " AND friend_id=" + std::to_string(friend_id));
}

bool RelationDao::addFriendBidirectional(uint64_t user_id, uint64_t friend_id) {
    if (user_id == 0 || friend_id == 0 || user_id == friend_id) return false;
    auto conn = pool_.acquire();
    if (!conn) return false;
    MySqlTransaction tx(*conn);
    if (!tx.active()) return false;
    if (!addFriendBidirectional(*conn, user_id, friend_id)) return false;
    return tx.commit();
}

bool RelationDao::addFriendBidirectional(MySqlConnection& conn, uint64_t user_id, uint64_t friend_id) {
    return addFriend(conn, user_id, friend_id) && addFriend(conn, friend_id, user_id);
}

bool RelationDao::deleteFriendBidirectional(uint64_t user_id, uint64_t friend_id) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    MySqlTransaction tx(*conn);
    if (!tx.active()) return false;
    std::string sql1 = "UPDATE friendships SET status=0, updated_at=UNIX_TIMESTAMP()*1000 WHERE user_id=" +
                       std::to_string(user_id) + " AND friend_id=" + std::to_string(friend_id);
    std::string sql2 = "UPDATE friendships SET status=0, updated_at=UNIX_TIMESTAMP()*1000 WHERE user_id=" +
                       std::to_string(friend_id) + " AND friend_id=" + std::to_string(user_id);
    if (!conn->executeUpdate(sql1) || !conn->executeUpdate(sql2)) return false;
    return tx.commit();
}

bool RelationDao::isFriend(uint64_t user_id, uint64_t friend_id) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    auto result = conn->executeQuery("SELECT id FROM friendships WHERE user_id=" + std::to_string(user_id) +
                                     " AND friend_id=" + std::to_string(friend_id) + " AND status=1 LIMIT 1");
    return result && result->next();
}

std::vector<uint64_t> RelationDao::listFriends(uint64_t user_id) {
    std::vector<uint64_t> friends;
    auto conn = pool_.acquire();
    if (!conn) return friends;
    auto result = conn->executeQuery("SELECT friend_id FROM friendships WHERE user_id=" + std::to_string(user_id) +
                                     " AND status=1 ORDER BY friend_id ASC");
    while (result && result->next()) friends.push_back(result->getUInt64("friend_id"));
    return friends;
}

}  // namespace nebula
