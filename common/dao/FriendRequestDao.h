#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nebula {

class MySqlConnection;
class MySqlConnectionPool;
class RelationDao;

enum class FriendRequestStatus {
    PENDING = 0,
    ACCEPTED = 1,
    REJECTED = 2,
    CANCELED = 3
};

struct FriendRequest {
    uint64_t id = 0;
    uint64_t request_id = 0;
    uint64_t from_user_id = 0;
    uint64_t to_user_id = 0;
    std::string message;
    int status = static_cast<int>(FriendRequestStatus::PENDING);
    int64_t created_at = 0;
    int64_t updated_at = 0;
};

class FriendRequestDao {
public:
    explicit FriendRequestDao(MySqlConnectionPool& pool);

    bool createRequest(const FriendRequest& request);
    std::optional<FriendRequest> getByRequestId(uint64_t request_id);
    bool updateStatus(MySqlConnection& conn, uint64_t request_id, FriendRequestStatus from_status, FriendRequestStatus to_status);
    bool updateStatus(uint64_t request_id, FriendRequestStatus from_status, FriendRequestStatus to_status);
    bool acceptRequestWithFriendship(uint64_t request_id, RelationDao& relation_dao);
    std::vector<FriendRequest> listReceived(uint64_t user_id, int status, size_t limit);
    std::vector<FriendRequest> listSent(uint64_t user_id, int status, size_t limit);

private:
    MySqlConnectionPool& pool_;
};

}  // namespace nebula
