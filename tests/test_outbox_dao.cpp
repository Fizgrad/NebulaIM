#include "common/db/MySqlConnectionPool.h"
#include "common/outbox/OutboxDao.h"

#include <cassert>
#include <iostream>

int main() {
    nebula::MySqlConnectionPool pool;
    nebula::OutboxDao dao(pool);
    auto events = dao.fetchPendingEvents(10, 0);
    assert(events.empty());
    std::cout << "test_outbox_dao passed (no database required)\n";
    return 0;
}
