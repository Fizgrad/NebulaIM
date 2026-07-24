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

bool FriendRequestDao::createRequest(FriendRequest* request) {
    if (request == nullptr) return false;
    auto conn = pool_.acquire();
    if (!conn) return false;
    int64_t now = request->created_at > 0 ? request->created_at : TimeUtil::nowMs();
    std::string sql = "INSERT INTO friend_requests(from_user_id,to_user_id,message,status,created_at,updated_at) VALUES(" +
                      std::to_string(request->from_user_id) + "," +
                      std::to_string(request->to_user_id) + ",'" + conn->escapeString(request->message) + "'," +
                      std::to_string(request->status) + "," + std::to_string(now) + "," +
                      std::to_string(request->updated_at > 0 ? request->updated_at : now) + ")";
    if (!conn->executeUpdate(sql)) return false;
    request->id = conn->lastInsertId();
    return request->id != 0;
}

bool FriendRequestDao::hasPendingBetween(uint64_t first_user_id, uint64_t second_user_id) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    auto result = conn->executeQuery(
        "SELECT id FROM friend_requests WHERE status=0 AND ((from_user_id=" +
        std::to_string(first_user_id) + " AND to_user_id=" + std::to_string(second_user_id) +
        ") OR (from_user_id=" + std::to_string(second_user_id) + " AND to_user_id=" +
        std::to_string(first_user_id) + ")) LIMIT 1");
    return result && result->next();
}

std::optional<FriendRequest> FriendRequestDao::getByRequestId(uint64_t request_id) {
    auto conn = pool_.acquire();
    if (!conn) return std::nullopt;
    auto result = conn->executeQuery("SELECT * FROM friend_requests WHERE id=" + std::to_string(request_id) + " LIMIT 1");
    if (!result || !result->next()) return std::nullopt;
    return parseFriendRequest(*result);
}

bool FriendRequestDao::updateStatus(MySqlConnection& conn,
                                    uint64_t request_id,
                                    FriendRequestStatus from_status,
                                    FriendRequestStatus to_status) {
    if (!conn.executeUpdate("UPDATE friend_requests SET status=" + std::to_string(static_cast<int>(to_status)) +
                            ", updated_at=" + std::to_string(TimeUtil::nowMs()) +
                            " WHERE id=" + std::to_string(request_id) +
                            " AND status=" + std::to_string(static_cast<int>(from_status)))) {
        return false;
    }
    return conn.affectedRows() > 0;
}

bool FriendRequestDao::updateStatus(uint64_t request_id, FriendRequestStatus from_status, FriendRequestStatus to_status) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    return updateStatus(*conn, request_id, from_status, to_status);
}

bool FriendRequestDao::acceptRequestWithFriendship(uint64_t request_id, RelationDao& relation_dao) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    auto result = conn->executeQuery("SELECT * FROM friend_requests WHERE id=" + std::to_string(request_id) + " LIMIT 1");
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
