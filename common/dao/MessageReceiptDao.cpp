#include "common/dao/MessageReceiptDao.h"

#include "common/db/MySqlConnection.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/db/MySqlResult.h"

namespace nebula {

MessageReceiptDao::MessageReceiptDao(MySqlConnectionPool& pool) : pool_(pool) {}

bool MessageReceiptDao::upsertDelivered(uint64_t message_id, uint64_t user_id, int64_t delivered_at) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    std::string sql = "INSERT INTO message_receipts(message_id,user_id,delivered_at,read_at,created_at,updated_at) VALUES(" +
                      std::to_string(message_id) + "," + std::to_string(user_id) + "," +
                      std::to_string(delivered_at) + ",0," + std::to_string(delivered_at) + "," +
                      std::to_string(delivered_at) +
                      ") ON DUPLICATE KEY UPDATE delivered_at=GREATEST(delivered_at,VALUES(delivered_at)), updated_at=VALUES(updated_at)";
    return conn->executeUpdate(sql);
}

bool MessageReceiptDao::markRead(uint64_t message_id, uint64_t user_id, int64_t read_at) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    std::string sql = "INSERT INTO message_receipts(message_id,user_id,delivered_at,read_at,created_at,updated_at) VALUES(" +
                      std::to_string(message_id) + "," + std::to_string(user_id) + "," +
                      std::to_string(read_at) + "," + std::to_string(read_at) + "," +
                      std::to_string(read_at) + "," + std::to_string(read_at) +
                      ") ON DUPLICATE KEY UPDATE delivered_at=GREATEST(delivered_at,VALUES(delivered_at)), read_at=GREATEST(read_at,VALUES(read_at)), updated_at=VALUES(updated_at)";
    return conn->executeUpdate(sql);
}

bool MessageReceiptDao::markConversationRead(uint64_t conversation_id, uint64_t user_id, int64_t read_at) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    std::string sql = "INSERT INTO message_receipts(message_id,user_id,delivered_at,read_at,created_at,updated_at) "
                      "SELECT message_id," + std::to_string(user_id) + "," + std::to_string(read_at) + "," +
                      std::to_string(read_at) + "," + std::to_string(read_at) + "," + std::to_string(read_at) +
                      " FROM messages WHERE conversation_id=" + std::to_string(conversation_id) +
                      " ON DUPLICATE KEY UPDATE delivered_at=GREATEST(delivered_at,VALUES(delivered_at)), read_at=GREATEST(read_at,VALUES(read_at)), updated_at=VALUES(updated_at)";
    return conn->executeUpdate(sql);
}

std::vector<MessageReadState> MessageReceiptDao::getReadState(uint64_t message_id) {
    std::vector<MessageReadState> states;
    auto conn = pool_.acquire();
    if (!conn) return states;
    auto result = conn->executeQuery("SELECT * FROM message_receipts WHERE message_id=" + std::to_string(message_id));
    while (result && result->next()) {
        MessageReadState state;
        state.message_id = result->getUInt64("message_id");
        state.user_id = result->getUInt64("user_id");
        state.delivered_at = result->getInt64("delivered_at");
        state.read_at = result->getInt64("read_at");
        states.push_back(state);
    }
    return states;
}

}  // namespace nebula
