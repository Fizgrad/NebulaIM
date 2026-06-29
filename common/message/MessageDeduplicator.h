#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace nebula {

class RedisClient;

class MessageDeduplicator {
public:
    MessageDeduplicator(RedisClient* redis_client, int ttl_seconds);

    std::optional<uint64_t> getExistingMessageId(uint64_t from_user_id, uint32_t client_sequence_id);
    bool markMessage(uint64_t from_user_id, uint32_t client_sequence_id, uint64_t message_id);
    std::string makeKey(uint64_t from_user_id, uint32_t client_sequence_id) const;

private:
    RedisClient* redis_client_;
    int ttl_seconds_;
};

}  // namespace nebula
