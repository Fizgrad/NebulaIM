#pragma once

#include <cstdint>
#include <vector>

namespace nebula {

class MySqlConnectionPool;
class MySqlConnection;

struct MessageReadState {
    uint64_t message_id = 0;
    uint64_t user_id = 0;
    int64_t delivered_at = 0;
    int64_t read_at = 0;
};

class MessageReceiptDao {
public:
    explicit MessageReceiptDao(MySqlConnectionPool& pool);

    bool upsertDelivered(uint64_t message_id, uint64_t user_id, int64_t delivered_at);
    bool upsertDelivered(MySqlConnection& conn, uint64_t message_id, uint64_t user_id, int64_t delivered_at);
    bool markRead(uint64_t message_id, uint64_t user_id, int64_t read_at);
    bool markRead(MySqlConnection& conn, uint64_t message_id, uint64_t user_id, int64_t read_at);
    bool markConversationRead(MySqlConnection& conn,
                              uint64_t conversation_id,
                              uint64_t user_id,
                              uint64_t up_to_message_id,
                              int64_t up_to_created_at,
                              int64_t read_at);
    std::vector<MessageReadState> getReadState(uint64_t message_id);

private:
    MySqlConnectionPool& pool_;
};

}  // namespace nebula
