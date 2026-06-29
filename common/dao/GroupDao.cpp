#include "common/dao/GroupDao.h"

#include "common/db/MySqlConnection.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/db/MySqlResult.h"
#include "common/db/MySqlTransaction.h"

namespace nebula {

namespace {
Group parseGroup(MySqlResult& result) {
    Group group;
    group.id = result.getUInt64("id");
    group.group_name = result.getString("group_name");
    group.owner_id = result.getUInt64("owner_id");
    group.created_at = result.getInt64("created_at");
    group.updated_at = result.getInt64("updated_at");
    return group;
}
}

GroupDao::GroupDao(MySqlConnectionPool& pool) : pool_(pool) {}

bool GroupDao::createGroup(const Group& group, uint64_t* group_id) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    MySqlTransaction tx(*conn);
    if (!tx.active()) return false;
    std::string sql = "INSERT INTO `groups`(group_name,owner_id,created_at,updated_at) VALUES('" +
                      conn->escapeString(group.group_name) + "'," + std::to_string(group.owner_id) + "," +
                      std::to_string(group.created_at) + "," + std::to_string(group.updated_at) + ")";
    if (!conn->executeUpdate(sql)) return false;
    uint64_t new_group_id = conn->lastInsertId();
    std::string member_sql = "INSERT INTO group_members(group_id,user_id,role,created_at,updated_at) VALUES(" +
                             std::to_string(new_group_id) + "," + std::to_string(group.owner_id) +
                             ",1," + std::to_string(group.created_at) + "," + std::to_string(group.updated_at) + ")";
    if (!conn->executeUpdate(member_sql)) return false;
    if (!tx.commit()) return false;
    if (group_id != nullptr) *group_id = new_group_id;
    return true;
}

std::optional<Group> GroupDao::getGroupById(uint64_t group_id) {
    auto conn = pool_.acquire();
    if (!conn) return std::nullopt;
    auto result = conn->executeQuery("SELECT * FROM `groups` WHERE id=" + std::to_string(group_id) + " LIMIT 1");
    if (!result || !result->next()) return std::nullopt;
    return parseGroup(*result);
}

bool GroupDao::addMember(uint64_t group_id, uint64_t user_id, int role) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    return conn->executeUpdate("INSERT INTO group_members(group_id,user_id,role,created_at,updated_at) VALUES(" +
                               std::to_string(group_id) + "," + std::to_string(user_id) + "," + std::to_string(role) +
                               ",UNIX_TIMESTAMP()*1000,UNIX_TIMESTAMP()*1000) ON DUPLICATE KEY UPDATE role=role");
}

bool GroupDao::removeMember(uint64_t group_id, uint64_t user_id) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    return conn->executeUpdate("DELETE FROM group_members WHERE group_id=" + std::to_string(group_id) + " AND user_id=" + std::to_string(user_id));
}

bool GroupDao::isMember(uint64_t group_id, uint64_t user_id) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    auto result = conn->executeQuery("SELECT id FROM group_members WHERE group_id=" + std::to_string(group_id) + " AND user_id=" + std::to_string(user_id) + " LIMIT 1");
    return result && result->next();
}

bool GroupDao::isOwner(uint64_t group_id, uint64_t user_id) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    auto result = conn->executeQuery("SELECT id FROM group_members WHERE group_id=" + std::to_string(group_id) +
                                     " AND user_id=" + std::to_string(user_id) + " AND role=1 LIMIT 1");
    return result && result->next();
}

std::vector<uint64_t> GroupDao::listMembers(uint64_t group_id) {
    std::vector<uint64_t> members;
    auto conn = pool_.acquire();
    if (!conn) return members;
    auto result = conn->executeQuery("SELECT user_id FROM group_members WHERE group_id=" + std::to_string(group_id) + " ORDER BY user_id ASC");
    while (result && result->next()) members.push_back(result->getUInt64("user_id"));
    return members;
}

}  // namespace nebula
