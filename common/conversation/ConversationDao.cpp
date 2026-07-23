#include "common/conversation/ConversationDao.h"

#include "common/db/MySqlConnection.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/db/MySqlResult.h"
#include "common/db/MySqlTransaction.h"
#include "common/utils/TimeUtil.h"

namespace nebula {

namespace {

Conversation parseConversation(MySqlResult& result) {
    Conversation conv;
    conv.id = result.getUInt64("id");
    conv.conversation_id = result.getUInt64("conversation_id");
    conv.conversation_type = result.getInt("conversation_type");
    conv.owner_user_id = result.getUInt64("owner_user_id");
    conv.peer_user_id = result.getUInt64("peer_user_id");
    conv.group_id = result.getUInt64("group_id");
    conv.last_message_id = result.getUInt64("last_message_id");
    conv.last_message_preview = result.getString("last_message_preview");
    conv.last_message_at = result.getInt64("last_message_at");
    conv.unread_count = result.getInt("unread_count");
    conv.pinned = result.getInt("pinned") != 0;
    conv.muted = result.getInt("muted") != 0;
    conv.deleted = result.getInt("deleted") != 0;
    conv.created_at = result.getInt64("created_at");
    conv.updated_at = result.getInt64("updated_at");
    return conv;
}

}  // namespace

ConversationDao::ConversationDao(MySqlConnectionPool& pool) : pool_(pool) {}

bool ConversationDao::upsertSingleConversationForUsers(uint64_t conversation_id,
                                                       uint64_t from_user_id,
                                                       uint64_t to_user_id,
                                                       uint64_t last_message_id,
                                                       const std::string& preview,
                                                       int64_t now_ms) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    return upsertSingleConversationForUsers(*conn, conversation_id, from_user_id, to_user_id, last_message_id, preview, now_ms);
}

bool ConversationDao::upsertSingleConversationForUsers(MySqlConnection& conn,
                                                       uint64_t conversation_id,
                                                       uint64_t from_user_id,
                                                       uint64_t to_user_id,
                                                       uint64_t last_message_id,
                                                       const std::string& preview,
                                                       int64_t now_ms) {
    Conversation sender;
    sender.conversation_id = conversation_id;
    sender.conversation_type = static_cast<int>(ConversationType::SINGLE);
    sender.owner_user_id = from_user_id;
    sender.peer_user_id = to_user_id;
    sender.last_message_id = last_message_id;
    sender.last_message_preview = preview;
    sender.last_message_at = now_ms;
    sender.created_at = now_ms;
    sender.updated_at = now_ms;

    Conversation receiver = sender;
    receiver.owner_user_id = to_user_id;
    receiver.peer_user_id = from_user_id;

    return upsertOne(conn, sender, false) && upsertOne(conn, receiver, true);
}

bool ConversationDao::upsertGroupConversationForMembers(uint64_t conversation_id,
                                                        uint64_t group_id,
                                                        uint64_t sender_user_id,
                                                        const std::vector<uint64_t>& member_ids,
                                                        uint64_t last_message_id,
                                                        const std::string& preview,
                                                        int64_t now_ms) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    MySqlTransaction tx(*conn);
    if (!tx.active()) return false;
    if (!upsertGroupConversationForMembers(*conn, conversation_id, group_id, sender_user_id, member_ids, last_message_id, preview, now_ms)) return false;
    return tx.commit();
}

bool ConversationDao::upsertGroupConversationForMembers(MySqlConnection& conn,
                                                        uint64_t conversation_id,
                                                        uint64_t group_id,
                                                        uint64_t sender_user_id,
                                                        const std::vector<uint64_t>& member_ids,
                                                        uint64_t last_message_id,
                                                        const std::string& preview,
                                                        int64_t now_ms) {
    for (uint64_t member_id : member_ids) {
        Conversation conv;
        conv.conversation_id = conversation_id;
        conv.conversation_type = static_cast<int>(ConversationType::GROUP);
        conv.owner_user_id = member_id;
        conv.group_id = group_id;
        conv.last_message_id = last_message_id;
        conv.last_message_preview = preview;
        conv.last_message_at = now_ms;
        conv.created_at = now_ms;
        conv.updated_at = now_ms;
        if (!upsertOne(conn, conv, member_id != sender_user_id)) return false;
    }
    return true;
}

std::vector<Conversation> ConversationDao::listConversations(uint64_t owner_user_id, size_t limit, size_t offset) {
    std::vector<Conversation> conversations;
    auto conn = pool_.acquire();
    if (!conn) return conversations;
    auto result = conn->executeQuery("SELECT * FROM conversations WHERE owner_user_id=" + std::to_string(owner_user_id) +
                                     " AND deleted=0 ORDER BY pinned DESC, updated_at DESC LIMIT " +
                                     std::to_string(limit) + " OFFSET " + std::to_string(offset));
    while (result && result->next()) conversations.push_back(parseConversation(*result));
    return conversations;
}

bool ConversationDao::isOwner(uint64_t owner_user_id, uint64_t conversation_id) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    auto result = conn->executeQuery("SELECT id FROM conversations WHERE owner_user_id=" + std::to_string(owner_user_id) +
                                     " AND conversation_id=" + std::to_string(conversation_id) +
                                     " AND deleted=0 LIMIT 1");
    return result && result->next();
}

bool ConversationDao::markRead(uint64_t owner_user_id, uint64_t conversation_id) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    if (!conn->executeUpdate("UPDATE conversations SET unread_count=0, updated_at=" + std::to_string(TimeUtil::nowMs()) +
                             " WHERE owner_user_id=" + std::to_string(owner_user_id) +
                             " AND conversation_id=" + std::to_string(conversation_id))) {
        return false;
    }
    return conn->affectedRows() > 0;
}

bool ConversationDao::deleteConversation(uint64_t owner_user_id, uint64_t conversation_id) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    if (!conn->executeUpdate("UPDATE conversations SET deleted=1, updated_at=" + std::to_string(TimeUtil::nowMs()) +
                             " WHERE owner_user_id=" + std::to_string(owner_user_id) +
                             " AND conversation_id=" + std::to_string(conversation_id))) {
        return false;
    }
    return conn->affectedRows() > 0;
}

bool ConversationDao::pinConversation(uint64_t owner_user_id, uint64_t conversation_id, bool pinned) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    if (!conn->executeUpdate("UPDATE conversations SET pinned=" + std::to_string(pinned ? 1 : 0) +
                             ", updated_at=" + std::to_string(TimeUtil::nowMs()) +
                             " WHERE owner_user_id=" + std::to_string(owner_user_id) +
                             " AND conversation_id=" + std::to_string(conversation_id))) {
        return false;
    }
    return conn->affectedRows() > 0;
}

bool ConversationDao::muteConversation(uint64_t owner_user_id, uint64_t conversation_id, bool muted) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    if (!conn->executeUpdate("UPDATE conversations SET muted=" + std::to_string(muted ? 1 : 0) +
                             ", updated_at=" + std::to_string(TimeUtil::nowMs()) +
                             " WHERE owner_user_id=" + std::to_string(owner_user_id) +
                             " AND conversation_id=" + std::to_string(conversation_id))) {
        return false;
    }
    return conn->affectedRows() > 0;
}

bool ConversationDao::updateLastMessage(uint64_t owner_user_id,
                                        uint64_t conversation_id,
                                        uint64_t last_message_id,
                                        const std::string& preview,
                                        int64_t now_ms) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    if (!conn->executeUpdate("UPDATE conversations SET last_message_id=" + std::to_string(last_message_id) +
                             ", last_message_preview='" + conn->escapeString(preview) +
                             "', last_message_at=" + std::to_string(now_ms) +
                             ", updated_at=" + std::to_string(now_ms) +
                             " WHERE owner_user_id=" + std::to_string(owner_user_id) +
                             " AND conversation_id=" + std::to_string(conversation_id))) {
        return false;
    }
    return conn->affectedRows() > 0;
}

bool ConversationDao::increaseUnreadCount(uint64_t owner_user_id, uint64_t conversation_id, int delta) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    if (!conn->executeUpdate("UPDATE conversations SET unread_count=GREATEST(0, unread_count+" + std::to_string(delta) +
                             "), updated_at=" + std::to_string(TimeUtil::nowMs()) +
                             " WHERE owner_user_id=" + std::to_string(owner_user_id) +
                             " AND conversation_id=" + std::to_string(conversation_id))) {
        return false;
    }
    return conn->affectedRows() > 0;
}

bool ConversationDao::upsertOne(MySqlConnection& conn, const Conversation& conversation, bool increase_unread) {
    int unread_inc = increase_unread ? 1 : 0;
    std::string sql = "INSERT INTO conversations(conversation_id,conversation_type,owner_user_id,peer_user_id,group_id,last_message_id,last_message_preview,last_message_at,unread_count,pinned,muted,deleted,created_at,updated_at) VALUES(" +
                      std::to_string(conversation.conversation_id) + "," +
                      std::to_string(conversation.conversation_type) + "," +
                      std::to_string(conversation.owner_user_id) + "," +
                      std::to_string(conversation.peer_user_id) + "," +
                      std::to_string(conversation.group_id) + "," +
                      std::to_string(conversation.last_message_id) + ",'" +
                      conn.escapeString(conversation.last_message_preview) + "'," +
                      std::to_string(conversation.last_message_at) + "," +
                      std::to_string(unread_inc) + ",0,0,0," +
                      std::to_string(conversation.created_at) + "," +
                      std::to_string(conversation.updated_at) +
                      ") ON DUPLICATE KEY UPDATE last_message_id=VALUES(last_message_id), last_message_preview=VALUES(last_message_preview), last_message_at=VALUES(last_message_at), unread_count=unread_count+" +
                      std::to_string(unread_inc) + ", deleted=0, updated_at=VALUES(updated_at)";
    return conn.executeUpdate(sql);
}

}  // namespace nebula
