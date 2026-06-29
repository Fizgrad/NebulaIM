#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nebula {

class MySqlConnectionPool;

struct Group {
    uint64_t id = 0;
    std::string group_name;
    uint64_t owner_id = 0;
    int64_t created_at = 0;
    int64_t updated_at = 0;
};

class GroupDao {
public:
    explicit GroupDao(MySqlConnectionPool& pool);

    bool createGroup(const Group& group, uint64_t* group_id);
    std::optional<Group> getGroupById(uint64_t group_id);
    bool addMember(uint64_t group_id, uint64_t user_id, int role);
    bool removeMember(uint64_t group_id, uint64_t user_id);
    bool isMember(uint64_t group_id, uint64_t user_id);
    bool isOwner(uint64_t group_id, uint64_t user_id);
    std::vector<uint64_t> listMembers(uint64_t group_id);

private:
    MySqlConnectionPool& pool_;
};

}  // namespace nebula
