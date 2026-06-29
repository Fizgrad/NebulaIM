#include "common/config/Config.h"

#include <cassert>
#include <string>

int main() {
    nebula::Config config;
    assert(config.loadFromFile("config/nebula.conf"));

    assert(config.has("server.host"));
    assert(config.getString("server.host") == "0.0.0.0");
    assert(config.getInt("server.port") == 9000);
    assert(config.getBool("log.console") == true);
    assert(config.getString("missing.key", "fallback") == "fallback");
    assert(config.getInt("missing.int", 42) == 42);
    assert(config.getBool("missing.bool", true) == true);
    assert(!config.has("not.exists"));

    return 0;
}
