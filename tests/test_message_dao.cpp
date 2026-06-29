#include "common/config/Config.h"
#include "common/dao/MessageDao.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/utils/TimeUtil.h"

#include <cassert>

int main() {
    nebula::Config config;
    assert(config.loadFromFile("config/nebula.conf"));

    nebula::MySqlConfig mysql;
    mysql.host = config.getString("mysql.host", mysql.host);
    mysql.port = config.getInt("mysql.port", mysql.port);
    mysql.user = config.getString("mysql.user", mysql.user);
    mysql.password = config.getString("mysql.password", mysql.password);
    mysql.database = config.getString("mysql.database", mysql.database);

    nebula::MySqlConnectionPool pool;
    assert(pool.init(mysql, 2));

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

    auto loaded = dao.getMessageById(msg.message_id);
    assert(loaded.has_value());
    assert(loaded->content == msg.content);

    auto list = dao.listConversationMessages(msg.conversation_id, now + 1000, 10);
    assert(!list.empty());

    assert(dao.updateMessageStatus(msg.message_id, 3));
    loaded = dao.getMessageById(msg.message_id);
    assert(loaded.has_value());
    assert(loaded->status == 3);

    return 0;
}
