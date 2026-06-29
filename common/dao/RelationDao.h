#pragma once

#include <cstdint>
#include <vector>

namespace nebula {

class MySqlConnectionPool;

class RelationDao {
public:
    explicit RelationDao(MySqlConnectionPool& pool);

    bool addFriend(uint64_t user_id, uint64_t friend_id);
    bool deleteFriend(uint64_t user_id, uint64_t friend_id);
    bool addFriendBidirectional(uint64_t user_id, uint64_t friend_id);
    bool deleteFriendBidirectional(uint64_t user_id, uint64_t friend_id);
    bool isFriend(uint64_t user_id, uint64_t friend_id);
    std::vector<uint64_t> listFriends(uint64_t user_id);

private:
    MySqlConnectionPool& pool_;
};

}  // namespace nebula
