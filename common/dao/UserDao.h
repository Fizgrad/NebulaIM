#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace nebula {

class MySqlConnectionPool;

struct User {
    uint64_t id = 0;
    std::string username;
    std::string password_hash;
    std::string nickname;
    std::string avatar;
    int64_t created_at = 0;
    int64_t updated_at = 0;
};

class UserDao {
public:
    explicit UserDao(MySqlConnectionPool& pool);

    bool createUser(const User& user, uint64_t* user_id);
    std::optional<User> getUserById(uint64_t user_id);
    std::optional<User> getUserByUsername(const std::string& username);
    bool updateUserProfile(uint64_t user_id, const std::string& nickname, const std::string& avatar);

private:
    MySqlConnectionPool& pool_;
};

}  // namespace nebula
