#include "common/gateway/GatewayOnlineManager.h"

#include "common/log/Logger.h"
#include "common/redis/RedisClient.h"

#include <exception>
#include <utility>

namespace nebula {

GatewayOnlineManager::GatewayOnlineManager(RedisClient* redis_client, std::string gateway_id, int online_ttl_seconds)
    : redis_client_(redis_client),
      gateway_id_(std::move(gateway_id)),
      online_ttl_seconds_(online_ttl_seconds > 0 ? online_ttl_seconds : 60),
      online_worker_(1, 10000) {
    online_worker_.start();
}

GatewayOnlineManager::~GatewayOnlineManager() {
    online_worker_.stop();
}

bool GatewayOnlineManager::setOnline(uint64_t user_id, const std::string& device_id, const std::string& connection_id) {
    if (redis_client_ == nullptr || user_id == 0 || device_id.empty() || connection_id.empty()) return false;
    redis_client_->sadd(devicesKey(user_id), device_id);
    redis_client_->expire(devicesKey(user_id), online_ttl_seconds_);
    bool ok = redis_client_->setEx(onlineKey(user_id, device_id), online_ttl_seconds_, gateway_id_) &&
              redis_client_->setEx(connKey(user_id, device_id), online_ttl_seconds_, connection_id);
    LOG_INFO("gateway online set user_id=" + std::to_string(user_id) +
             " device_id=" + device_id +
             " connection_id=" + connection_id +
             " ok=" + (ok ? "true" : "false"));
    return ok;
}

bool GatewayOnlineManager::refreshOnline(uint64_t user_id, const std::string& device_id, const std::string& connection_id) {
    if (redis_client_ == nullptr || user_id == 0 || device_id.empty() || connection_id.empty()) return false;
    auto conn = redis_client_->get(connKey(user_id, device_id));
    if (!conn.has_value() || conn.value() != connection_id) return false;
    redis_client_->expire(devicesKey(user_id), online_ttl_seconds_);
    return redis_client_->expire(onlineKey(user_id, device_id), online_ttl_seconds_) &&
           redis_client_->expire(connKey(user_id, device_id), online_ttl_seconds_);
}

bool GatewayOnlineManager::setOffline(uint64_t user_id, const std::string& device_id, const std::string& connection_id) {
    if (redis_client_ == nullptr || user_id == 0 || device_id.empty() || connection_id.empty()) return false;
    auto conn = redis_client_->get(connKey(user_id, device_id));
    if (conn.has_value() && conn.value() != connection_id) return true;
    redis_client_->del(onlineKey(user_id, device_id));
    redis_client_->del(connKey(user_id, device_id));
    redis_client_->srem(devicesKey(user_id), device_id);
    LOG_INFO("gateway online removed user_id=" + std::to_string(user_id) +
             " device_id=" + device_id +
             " connection_id=" + connection_id);
    return true;
}

void GatewayOnlineManager::refreshOnlineAsync(uint64_t user_id, std::string device_id, std::string connection_id) {
    submitOnlineTask([this, user_id, device_id = std::move(device_id), connection_id = std::move(connection_id)]() {
        refreshOnline(user_id, device_id, connection_id);
    });
}

void GatewayOnlineManager::setOfflineAsync(uint64_t user_id, std::string device_id, std::string connection_id) {
    submitOnlineTask([this, user_id, device_id = std::move(device_id), connection_id = std::move(connection_id)]() {
        setOffline(user_id, device_id, connection_id);
    });
}

void GatewayOnlineManager::submitOnlineTask(std::function<void()> task) {
    try {
        online_worker_.submit(std::move(task));
    } catch (const std::exception& ex) {
        LOG_ERROR(std::string("gateway online async task rejected: ") + ex.what());
    }
}

std::string GatewayOnlineManager::devicesKey(uint64_t user_id) const { return "nebula:user:devices:" + std::to_string(user_id); }
std::string GatewayOnlineManager::onlineKey(uint64_t user_id, const std::string& device_id) const { return "nebula:user:online:" + std::to_string(user_id) + ":" + device_id; }
std::string GatewayOnlineManager::connKey(uint64_t user_id, const std::string& device_id) const { return "nebula:user:conn:" + std::to_string(user_id) + ":" + device_id; }

}  // namespace nebula
