#include "common/redis/RedisClient.h"

#include "common/log/Logger.h"

#include <cstdarg>
#include <hiredis/hiredis.h>

namespace nebula {

RedisClient::RedisClient()
    : context_(nullptr) {}

RedisClient::~RedisClient() {
    close();
}

bool RedisClient::connect(const RedisConfig& config) {
    close();
    config_ = config;
    timeval timeout{};
    timeout.tv_sec = config.timeout_ms / 1000;
    timeout.tv_usec = (config.timeout_ms % 1000) * 1000;
    redisContext* ctx = redisConnectWithTimeout(config.host.c_str(), config.port, timeout);
    if (ctx == nullptr) {
        last_error_ = "redisConnectWithTimeout returned null";
        LOG_ERROR(last_error_);
        return false;
    }
    if (ctx->err) {
        last_error_ = ctx->errstr;
        LOG_ERROR("Redis connect failed: " + last_error_);
        redisFree(ctx);
        return false;
    }
    context_ = ctx;
    if (!config.password.empty()) {
        redisReply* reply = static_cast<redisReply*>(command("AUTH %s", config.password.c_str()));
        bool ok = checkStatus(reply);
        if (reply != nullptr) {
            freeReplyObject(reply);
        }
        if (!ok) {
            close();
            return false;
        }
    }
    return true;
}

void RedisClient::close() {
    if (context_ != nullptr) {
        redisFree(static_cast<redisContext*>(context_));
        context_ = nullptr;
    }
}

bool RedisClient::reconnect() {
    return connect(config_);
}

bool RedisClient::ping() {
    redisReply* reply = static_cast<redisReply*>(command("PING"));
    bool ok = reply != nullptr && reply->type == REDIS_REPLY_STATUS && std::string(reply->str) == "PONG";
    if (reply != nullptr) {
        freeReplyObject(reply);
    }
    return ok;
}

bool RedisClient::set(const std::string& key, const std::string& value) {
    redisReply* reply = static_cast<redisReply*>(command("SET %b %b", key.data(), key.size(), value.data(), value.size()));
    bool ok = checkStatus(reply);
    if (reply != nullptr) freeReplyObject(reply);
    return ok;
}

bool RedisClient::setEx(const std::string& key, int ttl_seconds, const std::string& value) {
    redisReply* reply = static_cast<redisReply*>(command("SETEX %b %d %b", key.data(), key.size(), ttl_seconds, value.data(), value.size()));
    bool ok = checkStatus(reply);
    if (reply != nullptr) freeReplyObject(reply);
    return ok;
}

std::optional<std::string> RedisClient::get(const std::string& key) {
    redisReply* reply = static_cast<redisReply*>(command("GET %b", key.data(), key.size()));
    auto value = stringReply(reply);
    if (reply != nullptr) freeReplyObject(reply);
    return value;
}

bool RedisClient::del(const std::string& key) {
    redisReply* reply = static_cast<redisReply*>(command("DEL %b", key.data(), key.size()));
    bool ok = reply != nullptr && reply->type == REDIS_REPLY_INTEGER && reply->integer >= 0;
    if (reply != nullptr) freeReplyObject(reply);
    return ok;
}

bool RedisClient::exists(const std::string& key) {
    redisReply* reply = static_cast<redisReply*>(command("EXISTS %b", key.data(), key.size()));
    bool ok = reply != nullptr && reply->type == REDIS_REPLY_INTEGER && reply->integer == 1;
    if (reply != nullptr) freeReplyObject(reply);
    return ok;
}

bool RedisClient::expire(const std::string& key, int ttl_seconds) {
    redisReply* reply = static_cast<redisReply*>(command("EXPIRE %b %d", key.data(), key.size(), ttl_seconds));
    bool ok = reply != nullptr && reply->type == REDIS_REPLY_INTEGER && reply->integer == 1;
    if (reply != nullptr) freeReplyObject(reply);
    return ok;
}

int64_t RedisClient::incr(const std::string& key) {
    redisReply* reply = static_cast<redisReply*>(command("INCR %b", key.data(), key.size()));
    int64_t value = reply != nullptr && reply->type == REDIS_REPLY_INTEGER ? reply->integer : -1;
    if (reply != nullptr) freeReplyObject(reply);
    return value;
}

bool RedisClient::hset(const std::string& key, const std::string& field, const std::string& value) {
    redisReply* reply = static_cast<redisReply*>(command("HSET %b %b %b", key.data(), key.size(), field.data(), field.size(), value.data(), value.size()));
    bool ok = reply != nullptr && reply->type == REDIS_REPLY_INTEGER && reply->integer >= 0;
    if (reply != nullptr) freeReplyObject(reply);
    return ok;
}

std::optional<std::string> RedisClient::hget(const std::string& key, const std::string& field) {
    redisReply* reply = static_cast<redisReply*>(command("HGET %b %b", key.data(), key.size(), field.data(), field.size()));
    auto value = stringReply(reply);
    if (reply != nullptr) freeReplyObject(reply);
    return value;
}

bool RedisClient::hdel(const std::string& key, const std::string& field) {
    redisReply* reply = static_cast<redisReply*>(command("HDEL %b %b", key.data(), key.size(), field.data(), field.size()));
    bool ok = reply != nullptr && reply->type == REDIS_REPLY_INTEGER && reply->integer >= 0;
    if (reply != nullptr) freeReplyObject(reply);
    return ok;
}

bool RedisClient::sadd(const std::string& key, const std::string& member) {
    redisReply* reply = static_cast<redisReply*>(command("SADD %b %b", key.data(), key.size(), member.data(), member.size()));
    bool ok = reply != nullptr && reply->type == REDIS_REPLY_INTEGER && reply->integer >= 0;
    if (reply != nullptr) freeReplyObject(reply);
    return ok;
}

bool RedisClient::srem(const std::string& key, const std::string& member) {
    redisReply* reply = static_cast<redisReply*>(command("SREM %b %b", key.data(), key.size(), member.data(), member.size()));
    bool ok = reply != nullptr && reply->type == REDIS_REPLY_INTEGER && reply->integer >= 0;
    if (reply != nullptr) freeReplyObject(reply);
    return ok;
}

int64_t RedisClient::scard(const std::string& key) {
    redisReply* reply = static_cast<redisReply*>(command("SCARD %b", key.data(), key.size()));
    int64_t value = reply != nullptr && reply->type == REDIS_REPLY_INTEGER ? reply->integer : -1;
    if (reply != nullptr) freeReplyObject(reply);
    return value;
}

std::vector<std::string> RedisClient::smembers(const std::string& key) {
    redisReply* reply = static_cast<redisReply*>(command("SMEMBERS %b", key.data(), key.size()));
    auto values = arrayReply(reply);
    if (reply != nullptr) freeReplyObject(reply);
    return values;
}

std::vector<std::string> RedisClient::scan(const std::string& pattern, size_t count) {
    std::vector<std::string> keys;
    std::string cursor = "0";
    do {
        redisReply* reply = static_cast<redisReply*>(
            command("SCAN %s MATCH %b COUNT %zu", cursor.c_str(), pattern.data(), pattern.size(), count));
        if (reply == nullptr || reply->type != REDIS_REPLY_ARRAY || reply->elements != 2) {
            if (reply != nullptr) freeReplyObject(reply);
            break;
        }
        redisReply* next_cursor = reply->element[0];
        redisReply* key_array = reply->element[1];
        if (next_cursor == nullptr || next_cursor->type != REDIS_REPLY_STRING ||
            key_array == nullptr || key_array->type != REDIS_REPLY_ARRAY) {
            freeReplyObject(reply);
            break;
        }
        cursor.assign(next_cursor->str, next_cursor->len);
        for (size_t i = 0; i < key_array->elements; ++i) {
            redisReply* item = key_array->element[i];
            if (item != nullptr && item->type == REDIS_REPLY_STRING) keys.emplace_back(item->str, item->len);
        }
        freeReplyObject(reply);
    } while (cursor != "0");
    return keys;
}

bool RedisClient::zadd(const std::string& key, double score, const std::string& member) {
    redisReply* reply = static_cast<redisReply*>(command("ZADD %b %f %b", key.data(), key.size(), score, member.data(), member.size()));
    bool ok = reply != nullptr && reply->type == REDIS_REPLY_INTEGER && reply->integer >= 0;
    if (reply != nullptr) freeReplyObject(reply);
    return ok;
}

std::vector<std::string> RedisClient::zrange(const std::string& key, int start, int stop) {
    redisReply* reply = static_cast<redisReply*>(command("ZRANGE %b %d %d", key.data(), key.size(), start, stop));
    auto values = arrayReply(reply);
    if (reply != nullptr) freeReplyObject(reply);
    return values;
}

std::string RedisClient::lastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

void* RedisClient::command(const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (context_ == nullptr) {
        last_error_ = "Redis is not connected";
        return nullptr;
    }
    va_list ap;
    va_start(ap, fmt);
    void* reply = redisvCommand(static_cast<redisContext*>(context_), fmt, ap);
    va_end(ap);
    if (reply == nullptr) {
        redisContext* ctx = static_cast<redisContext*>(context_);
        last_error_ = ctx->errstr[0] != '\0' ? ctx->errstr : "redis command failed";
        LOG_ERROR("Redis command failed: " + last_error_);
    }
    return reply;
}

bool RedisClient::checkStatus(void* reply_ptr) {
    redisReply* reply = static_cast<redisReply*>(reply_ptr);
    if (reply == nullptr) return false;
    if (reply->type == REDIS_REPLY_STATUS && std::string(reply->str) == "OK") return true;
    if (reply->type == REDIS_REPLY_ERROR) {
        std::lock_guard<std::mutex> lock(mutex_);
        last_error_ = reply->str;
    }
    return false;
}

std::optional<std::string> RedisClient::stringReply(void* reply_ptr) {
    redisReply* reply = static_cast<redisReply*>(reply_ptr);
    if (reply == nullptr || reply->type == REDIS_REPLY_NIL) return std::nullopt;
    if (reply->type == REDIS_REPLY_STRING || reply->type == REDIS_REPLY_STATUS) return std::string(reply->str, reply->len);
    return std::nullopt;
}

std::vector<std::string> RedisClient::arrayReply(void* reply_ptr) {
    std::vector<std::string> values;
    redisReply* reply = static_cast<redisReply*>(reply_ptr);
    if (reply == nullptr || reply->type != REDIS_REPLY_ARRAY) return values;
    for (size_t i = 0; i < reply->elements; ++i) {
        redisReply* item = reply->element[i];
        if (item != nullptr && item->type == REDIS_REPLY_STRING) values.emplace_back(item->str, item->len);
    }
    return values;
}

}  // namespace nebula
