#include "common/dao/MessageDao.h"

#include "common/db/MySqlConnection.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/db/MySqlResult.h"

namespace nebula {

namespace {
MessageRecord parseMessage(MySqlResult& result) {
    MessageRecord msg;
    msg.id = result.getUInt64("id");
    msg.message_id = result.getUInt64("message_id");
    msg.conversation_id = result.getUInt64("conversation_id");
    msg.from_user_id = result.getUInt64("from_user_id");
    msg.to_user_id = result.getUInt64("to_user_id");
    msg.group_id = result.getUInt64("group_id");
    msg.message_type = result.getInt("message_type");
    msg.content = result.getString("content");
    msg.status = result.getInt("status");
    msg.recalled = result.getInt("recalled") != 0;
    msg.recalled_at = result.getInt64("recalled_at");
    msg.created_at = result.getInt64("created_at");
    msg.updated_at = result.getInt64("updated_at");
    return msg;
}
}

MessageDao::MessageDao(MySqlConnectionPool& pool) : pool_(pool) {}

bool MessageDao::insertMessage(const MessageRecord& message) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    return insertMessage(*conn, message);
}

bool MessageDao::insertMessage(MySqlConnection& conn, const MessageRecord& message) {
    std::string sql = "INSERT INTO messages(message_id,conversation_id,from_user_id,to_user_id,group_id,message_type,content,status,recalled,recalled_at,created_at,updated_at) VALUES(" +
                      std::to_string(message.message_id) + "," + std::to_string(message.conversation_id) + "," +
                      std::to_string(message.from_user_id) + "," + std::to_string(message.to_user_id) + "," +
                      std::to_string(message.group_id) + "," + std::to_string(message.message_type) + ",'" +
                      conn.escapeString(message.content) + "'," + std::to_string(message.status) + "," +
                      std::to_string(message.recalled ? 1 : 0) + "," + std::to_string(message.recalled_at) + "," +
                      std::to_string(message.created_at) + "," + std::to_string(message.updated_at) + ")";
    return conn.executeUpdate(sql);
}

bool MessageDao::messageExists(uint64_t message_id) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    auto result = conn->executeQuery("SELECT id FROM messages WHERE message_id=" + std::to_string(message_id) + " LIMIT 1");
    return result && result->next();
}

std::optional<MessageRecord> MessageDao::getMessageById(uint64_t message_id) {
    auto conn = pool_.acquire();
    if (!conn) return std::nullopt;
    auto result = conn->executeQuery("SELECT * FROM messages WHERE message_id=" + std::to_string(message_id) + " LIMIT 1");
    if (!result || !result->next()) return std::nullopt;
    return parseMessage(*result);
}

std::vector<MessageRecord> MessageDao::listConversationMessages(uint64_t conversation_id, int64_t before_time, size_t limit) {
    std::vector<MessageRecord> messages;
    auto conn = pool_.acquire();
    if (!conn) return messages;
    auto result = conn->executeQuery("SELECT * FROM messages WHERE conversation_id=" + std::to_string(conversation_id) +
                                     " AND created_at<=" + std::to_string(before_time) +
                                     " ORDER BY created_at DESC, message_id DESC LIMIT " + std::to_string(limit));
    while (result && result->next()) messages.push_back(parseMessage(*result));
    return messages;
}

bool MessageDao::updateMessageStatus(uint64_t message_id, int status) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    if (!conn->executeUpdate("UPDATE messages SET status=" + std::to_string(status) +
                             ", updated_at=UNIX_TIMESTAMP()*1000 WHERE message_id=" + std::to_string(message_id))) {
        return false;
    }
    return messageExists(message_id);
}

bool MessageDao::recallMessage(uint64_t message_id, int64_t recalled_at) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    return conn->executeUpdate("UPDATE messages SET recalled=1, recalled_at=" + std::to_string(recalled_at) +
                               ", updated_at=" + std::to_string(recalled_at) +
                               " WHERE message_id=" + std::to_string(message_id) + " AND recalled=0");
}

}  // namespace nebula
