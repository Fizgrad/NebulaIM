#include "common/config/Config.h"
#include "common/redis/RedisClient.h"

#include <algorithm>
#include <cassert>

int main() {
    nebula::Config config;
    assert(config.loadFromFile("config/nebula.conf"));

    nebula::RedisConfig redis;
    redis.host = config.getString("redis.host", redis.host);
    redis.port = config.getInt("redis.port", redis.port);
    redis.timeout_ms = config.getInt("redis.timeout_ms", redis.timeout_ms);
    redis.password = config.getString("redis.password", redis.password);

    nebula::RedisClient client;
    assert(client.connect(redis));
    assert(client.ping());

    assert(client.set("nebula:test:string", "value"));
    assert(client.get("nebula:test:string").value() == "value");
    assert(client.setEx("nebula:test:ttl", 60, "ttl"));
    assert(client.exists("nebula:test:ttl"));
    assert(client.incr("nebula:test:counter") >= 1);
    assert(client.hset("nebula:test:hash", "field", "hvalue"));
    assert(client.hget("nebula:test:hash", "field").value() == "hvalue");
    assert(client.sadd("nebula:test:set", "member"));
    auto members = client.smembers("nebula:test:set");
    assert(std::find(members.begin(), members.end(), "member") != members.end());
    assert(client.zadd("nebula:test:zset", 1.0, "zmember"));
    auto zvalues = client.zrange("nebula:test:zset", 0, -1);
    assert(std::find(zvalues.begin(), zvalues.end(), "zmember") != zvalues.end());
    assert(client.del("nebula:test:string"));
    assert(client.hdel("nebula:test:hash", "field"));

    return 0;
}
