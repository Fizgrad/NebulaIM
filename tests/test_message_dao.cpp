#include "TestDeps.h"
#include "common/config/Config.h"
#include "common/dao/MessageDao.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/utils/TimeUtil.h"

#include <cassert>

int main() {
    nebula::MySqlConnectionPool pool;
    std::string reason;
    if (!nebula::tests::initMySqlPool(&pool, 2, &reason)) return nebula::tests::skip("test_message_dao", reason);

    int64_t now = nebula::TimeUtil::nowMs();
    nebula::MessageRecord msg;
    msg.message_id = static_cast<uint64_t>(now);
    msg.conversation_id = 70001;
    msg.from_user_id = 10001;
    msg.to_user_id = 10002;
    msg.message_type = 1;
    msg.content = "hello message dao";
    msg.status = 1;
    msg.created_at = now;
    msg.updated_at = now;

    nebula::MessageDao dao(pool);
    assert(dao.insertMessage(msg));
    nebula::MessageRecord same_timestamp = msg;
    same_timestamp.message_id += 1;
    same_timestamp.content = "same timestamp";
    assert(dao.insertMessage(same_timestamp));

    auto loaded = dao.getMessageById(msg.message_id);
    assert(loaded.has_value());
    assert(loaded->content == msg.content);

    auto list = dao.listConversationMessages(msg.conversation_id, 0, 0, 1);
    assert(list.size() == 1);
    assert(list.front().message_id == same_timestamp.message_id);
    auto older = dao.listConversationMessages(
        msg.conversation_id, list.front().created_at, list.front().message_id, 1);
    assert(older.size() == 1);
    assert(older.front().message_id == msg.message_id);

    assert(dao.updateMessageStatus(msg.message_id, 3));
    loaded = dao.getMessageById(msg.message_id);
    assert(loaded.has_value());
    assert(loaded->status == 3);

    return 0;
}
