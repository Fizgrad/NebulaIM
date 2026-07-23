#include "TestDeps.h"
#include "common/config/Config.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/db/MySqlResult.h"

#include <cassert>

int main() {
    nebula::MySqlConnectionPool pool;
    std::string reason;
    if (!nebula::tests::initMySqlPool(&pool, 2, &reason)) return nebula::tests::skip("test_mysql_pool", reason);
    {
        auto conn = pool.acquire();
        assert(conn);
        auto result = conn->executeQuery("SELECT 1 AS value");
        assert(result);
        assert(result->next());
        assert(result->getInt("value") == 1);
    }
    {
        auto c1 = pool.acquire();
        auto c2 = pool.acquire();
        assert(c1);
        assert(c2);
    }
    pool.stop();
    return 0;
}
