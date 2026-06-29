#include "common/push/OnlineStatusManager.h"

#include "common/log/Logger.h"
#include "common/redis/RedisClient.h"

namespace nebula {

OnlineStatusManager::OnlineStatusManager(RedisClient* redis_client) : redis_client_(redis_client) {}

OnlineStatus OnlineStatusManager::getOnlineStatus(uint64_t user_id) {
    OnlineStatus status;
    if (redis_client_ == nullptr || user_id == 0) return status;
    auto gateway = redis_client_->get(onlineKey(user_id));
    auto conn = redis_client_->get(connKey(user_id));
    if (!gateway.has_value() || !conn.has_value()) return status;
    status.online = true;
    status.gateway_id = gateway.value();
    status.connection_id = conn.value();
    return status;
}

bool OnlineStatusManager::setOnline(uint64_t user_id, const std::string& gateway_id, const std::string& connection_id, int ttl_seconds) {
    if (redis_client_ == nullptr || user_id == 0) return false;
    return redis_client_->setEx(onlineKey(user_id), ttl_seconds, gateway_id) && redis_client_->setEx(connKey(user_id), ttl_seconds, connection_id);
}

bool OnlineStatusManager::setOffline(uint64_t user_id) {
    if (redis_client_ == nullptr || user_id == 0) return false;
    bool a = redis_client_->del(onlineKey(user_id));
    bool b = redis_client_->del(connKey(user_id));
    return a && b;
}

std::string OnlineStatusManager::onlineKey(uint64_t user_id) const { return "nebula:user:online:" + std::to_string(user_id); }
std::string OnlineStatusManager::connKey(uint64_t user_id) const { return "nebula:user:conn:" + std::to_string(user_id); }

}  // namespace nebula
