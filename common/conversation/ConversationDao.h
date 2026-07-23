#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nebula {

class MySqlConnection;
class MySqlConnectionPool;

enum class ConversationType {
    SINGLE = 1,
    GROUP = 2
};

struct Conversation {
    uint64_t id = 0;
    uint64_t conversation_id = 0;
    int conversation_type = static_cast<int>(ConversationType::SINGLE);
    uint64_t owner_user_id = 0;
    uint64_t peer_user_id = 0;
    uint64_t group_id = 0;
    uint64_t last_message_id = 0;
    std::string last_message_preview;
    int64_t last_message_at = 0;
    int unread_count = 0;
    bool pinned = false;
    bool muted = false;
    bool deleted = false;
    int64_t created_at = 0;
    int64_t updated_at = 0;
};

class ConversationDao {
public:
    explicit ConversationDao(MySqlConnectionPool& pool);

    bool upsertSingleConversationForUsers(uint64_t conversation_id,
                                          uint64_t from_user_id,
                                          uint64_t to_user_id,
                                          uint64_t last_message_id,
                                          const std::string& preview,
                                          int64_t now_ms);
    bool upsertSingleConversationForUsers(MySqlConnection& conn,
                                          uint64_t conversation_id,
                                          uint64_t from_user_id,
                                          uint64_t to_user_id,
                                          uint64_t last_message_id,
                                          const std::string& preview,
                                          int64_t now_ms);
    bool upsertGroupConversationForMembers(uint64_t conversation_id,
                                           uint64_t group_id,
                                           uint64_t sender_user_id,
                                           const std::vector<uint64_t>& member_ids,
                                           uint64_t last_message_id,
                                           const std::string& preview,
                                           int64_t now_ms);
    bool upsertGroupConversationForMembers(MySqlConnection& conn,
                                           uint64_t conversation_id,
                                           uint64_t group_id,
                                           uint64_t sender_user_id,
                                           const std::vector<uint64_t>& member_ids,
                                           uint64_t last_message_id,
                                           const std::string& preview,
                                           int64_t now_ms);
    std::vector<Conversation> listConversations(uint64_t owner_user_id, size_t limit, size_t offset);
    bool isOwner(uint64_t owner_user_id, uint64_t conversation_id);
    bool markRead(uint64_t owner_user_id, uint64_t conversation_id);
    bool deleteConversation(uint64_t owner_user_id, uint64_t conversation_id);
    bool pinConversation(uint64_t owner_user_id, uint64_t conversation_id, bool pinned);
    bool muteConversation(uint64_t owner_user_id, uint64_t conversation_id, bool muted);
    bool updateLastMessage(uint64_t owner_user_id,
                           uint64_t conversation_id,
                           uint64_t last_message_id,
                           const std::string& preview,
                           int64_t now_ms);
    bool increaseUnreadCount(uint64_t owner_user_id, uint64_t conversation_id, int delta);

private:
    bool upsertOne(MySqlConnection& conn,
                   const Conversation& conversation,
                   bool increase_unread);

private:
    MySqlConnectionPool& pool_;
};

}  // namespace nebula
