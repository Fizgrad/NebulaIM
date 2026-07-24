#include "common/dao/GroupDao.h"

#include "common/db/MySqlConnection.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/db/MySqlResult.h"
#include "common/db/MySqlTransaction.h"

#include <algorithm>

namespace nebula {

namespace {
Group parseGroup(MySqlResult& result) {
    Group group;
    group.id = result.getUInt64("id");
    group.group_name = result.getString("group_name");
    group.owner_id = result.getUInt64("owner_id");
    group.member_count = static_cast<uint32_t>(result.getUInt64("member_count"));
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
    auto result = conn->executeQuery(
        "SELECT g.*, (SELECT COUNT(*) FROM group_members gm WHERE gm.group_id=g.id) AS member_count "
        "FROM `groups` g WHERE g.id=" + std::to_string(group_id) + " LIMIT 1");
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

std::vector<Group> GroupDao::listGroupsForUser(uint64_t user_id, size_t limit) {
    std::vector<Group> groups;
    auto conn = pool_.acquire();
    if (!conn || user_id == 0) return groups;
    size_t safe_limit = std::max<size_t>(1, std::min<size_t>(limit, 200));
    auto result = conn->executeQuery(
        "SELECT g.*, COUNT(all_members.user_id) AS member_count "
        "FROM group_members current_member "
        "INNER JOIN `groups` g ON g.id=current_member.group_id "
        "LEFT JOIN group_members all_members ON all_members.group_id=g.id "
        "WHERE current_member.user_id=" + std::to_string(user_id) +
        " GROUP BY g.id,g.group_name,g.owner_id,g.created_at,g.updated_at "
        "ORDER BY g.updated_at DESC,g.id DESC LIMIT " + std::to_string(safe_limit));
    while (result && result->next()) groups.push_back(parseGroup(*result));
    return groups;
}

std::vector<Group> GroupDao::searchGroups(const std::string& query, size_t limit) {
    std::vector<Group> groups;
    auto conn = pool_.acquire();
    if (!conn || query.empty()) return groups;
    size_t safe_limit = std::max<size_t>(1, std::min<size_t>(limit, 50));
    uint64_t exact_id = 0;
    try {
        size_t consumed = 0;
        exact_id = std::stoull(query, &consumed);
        if (consumed != query.size()) exact_id = 0;
    } catch (...) {
        exact_id = 0;
    }
    const std::string escaped = conn->escapeString(query);
    std::string where = "g.group_name LIKE '%" + escaped + "%'";
    if (exact_id != 0) where += " OR g.id=" + std::to_string(exact_id);
    auto result = conn->executeQuery(
        "SELECT g.*, COUNT(gm.user_id) AS member_count FROM `groups` g "
        "LEFT JOIN group_members gm ON gm.group_id=g.id WHERE " + where +
        " GROUP BY g.id,g.group_name,g.owner_id,g.created_at,g.updated_at "
        "ORDER BY CASE WHEN g.group_name='" + escaped + "' THEN 0 "
        "WHEN g.group_name LIKE '" + escaped + "%' THEN 1 ELSE 2 END,"
        "g.updated_at DESC,g.id DESC LIMIT " + std::to_string(safe_limit));
    while (result && result->next()) groups.push_back(parseGroup(*result));
    return groups;
}

}  // namespace nebula
