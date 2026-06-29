#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nebula {

class MySqlConnectionPool;
class MySqlConnection;

struct MessageRecord {
    uint64_t id = 0;
    uint64_t message_id = 0;
    uint64_t conversation_id = 0;
    uint64_t from_user_id = 0;
    uint64_t to_user_id = 0;
    uint64_t group_id = 0;
    int message_type = 1;
    std::string content;
    int status = 1;
    bool recalled = false;
    int64_t recalled_at = 0;
    int64_t created_at = 0;
    int64_t updated_at = 0;
};

class MessageDao {
public:
    explicit MessageDao(MySqlConnectionPool& pool);

    bool insertMessage(const MessageRecord& message);
    bool insertMessage(MySqlConnection& conn, const MessageRecord& message);
    bool messageExists(uint64_t message_id);
    std::optional<MessageRecord> getMessageById(uint64_t message_id);
    std::vector<MessageRecord> listConversationMessages(uint64_t conversation_id, int64_t before_time, size_t limit);
    bool updateMessageStatus(uint64_t message_id, int status);
    bool recallMessage(uint64_t message_id, int64_t recalled_at);
    bool recallMessage(MySqlConnection& conn, uint64_t message_id, int64_t recalled_at);

private:
    MySqlConnectionPool& pool_;
};

}  // namespace nebula
