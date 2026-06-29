#include "common/message/MessageDeduplicator.h"

#include "common/log/Logger.h"
#include "common/redis/RedisClient.h"

namespace nebula {

MessageDeduplicator::MessageDeduplicator(RedisClient* redis_client, int ttl_seconds)
    : redis_client_(redis_client), ttl_seconds_(ttl_seconds > 0 ? ttl_seconds : 86400) {}

std::optional<uint64_t> MessageDeduplicator::getExistingMessageId(uint64_t from_user_id, uint32_t client_sequence_id) {
    if (client_sequence_id == 0) return std::nullopt;
    if (redis_client_ == nullptr) return std::nullopt;
    auto value = redis_client_->get(makeKey(from_user_id, client_sequence_id));
    if (!value.has_value()) return std::nullopt;
    try {
        return static_cast<uint64_t>(std::stoull(value.value()));
    } catch (...) {
        LOG_ERROR("invalid dedup message id value");
        return std::nullopt;
    }
}

bool MessageDeduplicator::markMessage(uint64_t from_user_id, uint32_t client_sequence_id, uint64_t message_id) {
    if (client_sequence_id == 0) return true;
    if (redis_client_ == nullptr) return false;
    return redis_client_->setEx(makeKey(from_user_id, client_sequence_id), ttl_seconds_, std::to_string(message_id));
}

std::string MessageDeduplicator::makeKey(uint64_t from_user_id, uint32_t client_sequence_id) const {
    return "nebula:msg:dedup:" + std::to_string(from_user_id) + ":" + std::to_string(client_sequence_id);
}

}  // namespace nebula
