#include "common/dao/OfflineMessageDao.h"

#include "common/db/MySqlConnection.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/db/MySqlResult.h"

namespace nebula {

namespace {

std::string toHex(const std::string& bytes) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (unsigned char ch : bytes) {
        out.push_back(digits[ch >> 4]);
        out.push_back(digits[ch & 0x0F]);
    }
    return out;
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string fromHex(const std::string& hex) {
    if (hex.size() % 2 != 0) return "";
    std::string out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        int hi = hexValue(hex[i]);
        int lo = hexValue(hex[i + 1]);
        if (hi < 0 || lo < 0) return "";
        out.push_back(static_cast<char>((hi << 4) | lo));
    }
    return out;
}

OfflineMessage parseOffline(MySqlResult& result) {
    OfflineMessage msg;
    msg.id = result.getUInt64("id");
    msg.user_id = result.getUInt64("user_id");
    msg.message_id = result.getUInt64("message_id");
    msg.payload = fromHex(result.getString("payload"));
    msg.status = result.getInt("status");
    msg.created_at = result.getInt64("created_at");
    msg.updated_at = result.getInt64("updated_at");
    return msg;
}
}

OfflineMessageDao::OfflineMessageDao(MySqlConnectionPool& pool) : pool_(pool) {}

bool OfflineMessageDao::insertOfflineMessage(const OfflineMessage& message) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    std::string sql = "INSERT INTO offline_messages(user_id,message_id,payload,status,created_at,updated_at) VALUES(" +
                      std::to_string(message.user_id) + "," + std::to_string(message.message_id) + ",'" +
                      conn->escapeString(toHex(message.payload)) + "'," + std::to_string(message.status) + "," +
                      std::to_string(message.created_at) + "," + std::to_string(message.updated_at) +
                      ") ON DUPLICATE KEY UPDATE payload=IF(status=2,payload,VALUES(payload)), status=IF(status=2,status,VALUES(status)), updated_at=IF(status=2,updated_at,VALUES(updated_at))";
    return conn->executeUpdate(sql);
}

std::vector<OfflineMessage> OfflineMessageDao::listOfflineMessages(uint64_t user_id, size_t limit) {
    std::vector<OfflineMessage> messages;
    auto conn = pool_.acquire();
    if (!conn) return messages;
    auto result = conn->executeQuery("SELECT * FROM offline_messages WHERE user_id=" + std::to_string(user_id) +
                                     " AND status IN (0,1) ORDER BY created_at ASC, message_id ASC LIMIT " + std::to_string(limit));
    while (result && result->next()) messages.push_back(parseOffline(*result));
    return messages;
}

bool OfflineMessageDao::markAsPulled(uint64_t user_id, uint64_t message_id) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    return conn->executeUpdate("UPDATE offline_messages SET status=1, updated_at=UNIX_TIMESTAMP()*1000 WHERE user_id=" +
                               std::to_string(user_id) + " AND message_id=" + std::to_string(message_id) +
                               " AND status IN (0,1)");
}

bool OfflineMessageDao::markBatchAsPulled(uint64_t user_id, const std::vector<uint64_t>& message_ids) {
    if (message_ids.empty()) return true;
    auto conn = pool_.acquire();
    if (!conn) return false;
    std::string ids;
    for (size_t i = 0; i < message_ids.size(); ++i) {
        if (i > 0) ids += ",";
        ids += std::to_string(message_ids[i]);
    }
    return conn->executeUpdate("UPDATE offline_messages SET status=1, updated_at=UNIX_TIMESTAMP()*1000 WHERE user_id=" +
                               std::to_string(user_id) + " AND message_id IN (" + ids + ") AND status IN (0,1)");
}

bool OfflineMessageDao::markAsAcked(uint64_t user_id, uint64_t message_id) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    return conn->executeUpdate("UPDATE offline_messages SET status=2, updated_at=UNIX_TIMESTAMP()*1000 WHERE user_id=" +
                               std::to_string(user_id) + " AND message_id=" + std::to_string(message_id) +
                               " AND status IN (0,1,2)");
}

bool OfflineMessageDao::deleteAckedMessages(uint64_t user_id) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    return conn->executeUpdate("DELETE FROM offline_messages WHERE user_id=" + std::to_string(user_id) + " AND status=2");
}

}  // namespace nebula
