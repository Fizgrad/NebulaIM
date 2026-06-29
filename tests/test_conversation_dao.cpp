#include "common/conversation/ConversationDao.h"
#include "common/db/MySqlConnectionPool.h"

#include <cassert>
#include <iostream>

int main() {
    nebula::MySqlConnectionPool pool;
    nebula::ConversationDao dao(pool);
    auto rows = dao.listConversations(1, 20, 0);
    assert(rows.empty());
    std::cout << "test_conversation_dao passed (no database required)\n";
    return 0;
}
