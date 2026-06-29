#include "common/dao/FriendRequestDao.h"

#include "common/dao/RelationDao.h"
#include "common/db/MySqlConnection.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/db/MySqlResult.h"
#include "common/db/MySqlTransaction.h"
#include "common/utils/TimeUtil.h"

namespace nebula {

namespace {

FriendRequest parseFriendRequest(MySqlResult& result) {
    FriendRequest request;
    request.id = result.getUInt64("id");
    request.request_id = result.getUInt64("request_id");
    request.from_user_id = result.getUInt64("from_user_id");
    request.to_user_id = result.getUInt64("to_user_id");
    request.message = result.getString("message");
    request.status = result.getInt("status");
    request.created_at = result.getInt64("created_at");
    request.updated_at = result.getInt64("updated_at");
    return request;
}

}  // namespace

FriendRequestDao::FriendRequestDao(MySqlConnectionPool& pool) : pool_(pool) {}

bool FriendRequestDao::createRequest(const FriendRequest& request) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    int64_t now = request.created_at > 0 ? request.created_at : TimeUtil::nowMs();
    std::string sql = "INSERT INTO friend_requests(request_id,from_user_id,to_user_id,message,status,created_at,updated_at) VALUES(" +
                      std::to_string(request.request_id) + "," + std::to_string(request.from_user_id) + "," +
                      std::to_string(request.to_user_id) + ",'" + conn->escapeString(request.message) + "'," +
                      std::to_string(request.status) + "," + std::to_string(now) + "," +
                      std::to_string(request.updated_at > 0 ? request.updated_at : now) + ")";
    return conn->executeUpdate(sql);
}

std::optional<FriendRequest> FriendRequestDao::getByRequestId(uint64_t request_id) {
    auto conn = pool_.acquire();
    if (!conn) return std::nullopt;
    auto result = conn->executeQuery("SELECT * FROM friend_requests WHERE request_id=" + std::to_string(request_id) + " LIMIT 1");
    if (!result || !result->next()) return std::nullopt;
    return parseFriendRequest(*result);
}

bool FriendRequestDao::updateStatus(MySqlConnection& conn,
                                    uint64_t request_id,
                                    FriendRequestStatus from_status,
                                    FriendRequestStatus to_status) {
    return conn.executeUpdate("UPDATE friend_requests SET status=" + std::to_string(static_cast<int>(to_status)) +
                              ", updated_at=" + std::to_string(TimeUtil::nowMs()) +
                              " WHERE request_id=" + std::to_string(request_id) +
                              " AND status=" + std::to_string(static_cast<int>(from_status)));
}

bool FriendRequestDao::updateStatus(uint64_t request_id, FriendRequestStatus from_status, FriendRequestStatus to_status) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    return updateStatus(*conn, request_id, from_status, to_status);
}

bool FriendRequestDao::acceptRequestWithFriendship(uint64_t request_id, RelationDao& relation_dao) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    auto result = conn->executeQuery("SELECT * FROM friend_requests WHERE request_id=" + std::to_string(request_id) + " LIMIT 1");
    if (!result || !result->next()) return false;
    FriendRequest request = parseFriendRequest(*result);
    if (request.status != static_cast<int>(FriendRequestStatus::PENDING)) return false;

    MySqlTransaction tx(*conn);
    if (!tx.active()) return false;
    if (!updateStatus(*conn, request_id, FriendRequestStatus::PENDING, FriendRequestStatus::ACCEPTED)) return false;
    if (!relation_dao.addFriendBidirectional(*conn, request.from_user_id, request.to_user_id)) return false;
    return tx.commit();
}

std::vector<FriendRequest> FriendRequestDao::listReceived(uint64_t user_id, int status, size_t limit) {
    std::vector<FriendRequest> requests;
    auto conn = pool_.acquire();
    if (!conn) return requests;
    std::string sql = "SELECT * FROM friend_requests WHERE to_user_id=" + std::to_string(user_id);
    if (status >= 0) sql += " AND status=" + std::to_string(status);
    sql += " ORDER BY created_at DESC LIMIT " + std::to_string(limit);
    auto result = conn->executeQuery(sql);
    while (result && result->next()) requests.push_back(parseFriendRequest(*result));
    return requests;
}

std::vector<FriendRequest> FriendRequestDao::listSent(uint64_t user_id, int status, size_t limit) {
    std::vector<FriendRequest> requests;
    auto conn = pool_.acquire();
    if (!conn) return requests;
    std::string sql = "SELECT * FROM friend_requests WHERE from_user_id=" + std::to_string(user_id);
    if (status >= 0) sql += " AND status=" + std::to_string(status);
    sql += " ORDER BY created_at DESC LIMIT " + std::to_string(limit);
    auto result = conn->executeQuery(sql);
    while (result && result->next()) requests.push_back(parseFriendRequest(*result));
    return requests;
}

}  // namespace nebula
