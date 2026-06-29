#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace nebula {

struct RedisConfig {
    std::string host = "127.0.0.1";
    int port = 6379;
    int timeout_ms = 3000;
    std::string password;
};

class RedisClient {
public:
    RedisClient();
    ~RedisClient();

    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;

    bool connect(const RedisConfig& config);
    void close();
    bool reconnect();

    bool ping();

    bool set(const std::string& key, const std::string& value);
    bool setEx(const std::string& key, int ttl_seconds, const std::string& value);
    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    bool exists(const std::string& key);
    bool expire(const std::string& key, int ttl_seconds);
    int64_t incr(const std::string& key);

    bool hset(const std::string& key, const std::string& field, const std::string& value);
    std::optional<std::string> hget(const std::string& key, const std::string& field);
    bool hdel(const std::string& key, const std::string& field);

    bool sadd(const std::string& key, const std::string& member);
    bool srem(const std::string& key, const std::string& member);
    int64_t scard(const std::string& key);
    std::vector<std::string> smembers(const std::string& key);
    std::vector<std::string> scan(const std::string& pattern, size_t count = 1000);

    bool zadd(const std::string& key, double score, const std::string& member);
    std::vector<std::string> zrange(const std::string& key, int start, int stop);

    std::string lastError() const;

private:
    void* command(const char* fmt, ...);
    bool checkStatus(void* reply);
    std::optional<std::string> stringReply(void* reply);
    std::vector<std::string> arrayReply(void* reply);

private:
    void* context_;
    RedisConfig config_;
    std::string last_error_;
    mutable std::mutex mutex_;
};

}  // namespace nebula
