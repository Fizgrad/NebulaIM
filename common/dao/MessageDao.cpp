#include "common/dao/MessageDao.h"

#include "common.pb.h"
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
    msg.client_sequence_id = static_cast<uint32_t>(result.getUInt64("client_sequence_id"));
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
    const std::string sequence_value = message.client_sequence_id == 0
                                           ? "NULL"
                                           : std::to_string(message.client_sequence_id);
    std::string sql = "INSERT INTO messages(message_id,conversation_id,from_user_id,client_sequence_id,to_user_id,group_id,message_type,content,status,recalled,recalled_at,created_at,updated_at) VALUES(" +
                      std::to_string(message.message_id) + "," + std::to_string(message.conversation_id) + "," +
                      std::to_string(message.from_user_id) + "," + sequence_value + "," +
                      std::to_string(message.to_user_id) + "," +
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

std::optional<MessageRecord> MessageDao::getMessageByClientSequence(uint64_t from_user_id, uint32_t client_sequence_id) {
    if (from_user_id == 0 || client_sequence_id == 0) return std::nullopt;
    auto conn = pool_.acquire();
    if (!conn) return std::nullopt;
    auto result = conn->executeQuery("SELECT * FROM messages WHERE from_user_id=" + std::to_string(from_user_id) +
                                     " AND client_sequence_id=" + std::to_string(client_sequence_id) + " LIMIT 1");
    if (!result || !result->next()) return std::nullopt;
    return parseMessage(*result);
}

std::vector<MessageRecord> MessageDao::listConversationMessages(uint64_t conversation_id,
                                                                int64_t before_time,
                                                                uint64_t before_message_id,
                                                                size_t limit) {
    std::vector<MessageRecord> messages;
    auto conn = pool_.acquire();
    if (!conn) return messages;
    std::string cursor_clause;
    if (before_time > 0 && before_message_id > 0) {
        cursor_clause = " AND (created_at<" + std::to_string(before_time) +
                        " OR (created_at=" + std::to_string(before_time) +
                        " AND message_id<" + std::to_string(before_message_id) + "))";
    } else if (before_time > 0) {
        cursor_clause = " AND created_at<" + std::to_string(before_time);
    }
    auto result = conn->executeQuery("SELECT * FROM messages WHERE conversation_id=" + std::to_string(conversation_id) +
                                     cursor_clause +
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
    return recallMessage(*conn, message_id, recalled_at);
}

bool MessageDao::recallMessage(MySqlConnection& conn, uint64_t message_id, int64_t recalled_at) {
    if (!conn.executeUpdate("UPDATE messages SET recalled=1, recalled_at=" + std::to_string(recalled_at) +
                            ", status=" + std::to_string(static_cast<int>(proto::MESSAGE_STATUS_RECALLED)) +
                            ", updated_at=" + std::to_string(recalled_at) +
                            " WHERE message_id=" + std::to_string(message_id) + " AND recalled=0")) {
        return false;
    }
    return conn.affectedRows() > 0;
}

}  // namespace nebula
