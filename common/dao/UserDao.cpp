#include "common/dao/UserDao.h"

#include "common/db/MySqlConnection.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/db/MySqlResult.h"

namespace nebula {

namespace {
User parseUser(MySqlResult& result) {
    User user;
    user.id = result.getUInt64("id");
    user.username = result.getString("username");
    user.password_hash = result.getString("password_hash");
    user.nickname = result.getString("nickname");
    user.avatar = result.getString("avatar");
    user.created_at = result.getInt64("created_at");
    user.updated_at = result.getInt64("updated_at");
    return user;
}
}

UserDao::UserDao(MySqlConnectionPool& pool) : pool_(pool) {}

bool UserDao::createUser(const User& user, uint64_t* user_id) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    std::string sql = "INSERT INTO users(username,password_hash,nickname,avatar,created_at,updated_at) VALUES('" +
                      conn->escapeString(user.username) + "','" + conn->escapeString(user.password_hash) + "','" +
                      conn->escapeString(user.nickname) + "','" + conn->escapeString(user.avatar) + "'," +
                      std::to_string(user.created_at) + "," + std::to_string(user.updated_at) + ")";
    if (!conn->executeUpdate(sql)) return false;
    if (user_id != nullptr) *user_id = conn->lastInsertId();
    return true;
}

std::optional<User> UserDao::getUserById(uint64_t user_id) {
    auto conn = pool_.acquire();
    if (!conn) return std::nullopt;
    auto result = conn->executeQuery("SELECT * FROM users WHERE id=" + std::to_string(user_id) + " LIMIT 1");
    if (!result || !result->next()) return std::nullopt;
    return parseUser(*result);
}

std::optional<User> UserDao::getUserByUsername(const std::string& username) {
    auto conn = pool_.acquire();
    if (!conn) return std::nullopt;
    auto result = conn->executeQuery("SELECT * FROM users WHERE username='" + conn->escapeString(username) + "' LIMIT 1");
    if (!result || !result->next()) return std::nullopt;
    return parseUser(*result);
}

bool UserDao::updateUserProfile(uint64_t user_id, const std::string& nickname, const std::string& avatar) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    std::string sql = "UPDATE users SET nickname='" + conn->escapeString(nickname) + "', avatar='" +
                      conn->escapeString(avatar) + "', updated_at=UNIX_TIMESTAMP()*1000 WHERE id=" + std::to_string(user_id);
    return conn->executeUpdate(sql);
}

}  // namespace nebula
