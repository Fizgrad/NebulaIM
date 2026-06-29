#include "common/config/Config.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/db/MySqlResult.h"

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
