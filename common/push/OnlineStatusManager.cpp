#include "common/push/OnlineStatusManager.h"

#include "common/log/Logger.h"
#include "common/redis/RedisClient.h"

namespace nebula {

OnlineStatusManager::OnlineStatusManager(RedisClient* redis_client) : redis_client_(redis_client) {}

OnlineStatus OnlineStatusManager::getOnlineStatus(uint64_t user_id) {
    OnlineStatus status;
    if (redis_client_ == nullptr || user_id == 0) return status;
    auto statuses = getOnlineStatuses(user_id);
    if (!statuses.empty()) status = statuses.front();
    return status;
}

std::vector<OnlineStatus> OnlineStatusManager::getOnlineStatuses(uint64_t user_id) {
    std::vector<OnlineStatus> statuses;
    if (redis_client_ == nullptr || user_id == 0) return statuses;
    auto devices = redis_client_->smembers(devicesKey(user_id));
    for (const auto& device_id : devices) {
        auto gateway = redis_client_->get(onlineKey(user_id, device_id));
        auto conn = redis_client_->get(connKey(user_id, device_id));
        if (!gateway.has_value() || !conn.has_value()) {
            redis_client_->srem(devicesKey(user_id), device_id);
            continue;
        }
        OnlineStatus status;
        status.online = true;
        status.gateway_id = gateway.value();
        status.connection_id = conn.value();
        status.device_id = device_id;
        statuses.push_back(status);
    }
    LOG_INFO("online status lookup user_id=" + std::to_string(user_id) +
             " devices=" + std::to_string(devices.size()) +
             " online=" + std::to_string(statuses.size()));
    return statuses;
}

bool OnlineStatusManager::setOnline(uint64_t user_id, const std::string& device_id, const std::string& gateway_id, const std::string& connection_id, int ttl_seconds) {
    if (redis_client_ == nullptr || user_id == 0 || device_id.empty() || gateway_id.empty() || connection_id.empty()) return false;
    redis_client_->sadd(devicesKey(user_id), device_id);
    redis_client_->expire(devicesKey(user_id), ttl_seconds);
    return redis_client_->setEx(onlineKey(user_id, device_id), ttl_seconds, gateway_id) &&
           redis_client_->setEx(connKey(user_id, device_id), ttl_seconds, connection_id);
}

bool OnlineStatusManager::setOffline(uint64_t user_id) {
    if (redis_client_ == nullptr || user_id == 0) return false;
    auto devices = redis_client_->smembers(devicesKey(user_id));
    bool ok = true;
    for (const auto& device_id : devices) {
        ok = redis_client_->del(onlineKey(user_id, device_id)) && ok;
        ok = redis_client_->del(connKey(user_id, device_id)) && ok;
    }
    redis_client_->del(devicesKey(user_id));
    return ok;
}

std::string OnlineStatusManager::devicesKey(uint64_t user_id) const { return "nebula:user:devices:" + std::to_string(user_id); }
std::string OnlineStatusManager::onlineKey(uint64_t user_id, const std::string& device_id) const { return "nebula:user:online:" + std::to_string(user_id) + ":" + device_id; }
std::string OnlineStatusManager::connKey(uint64_t user_id, const std::string& device_id) const { return "nebula:user:conn:" + std::to_string(user_id) + ":" + device_id; }

}  // namespace nebula
