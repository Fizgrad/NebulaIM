#pragma once

#include "common/thread/ThreadPool.h"

#include <cstdint>
#include <functional>
#include <string>

namespace nebula {

class RedisClient;

class GatewayOnlineManager {
public:
    GatewayOnlineManager(RedisClient* redis_client, std::string gateway_id, int online_ttl_seconds);
    ~GatewayOnlineManager();

    bool setOnline(uint64_t user_id, const std::string& device_id, const std::string& connection_id);
    bool refreshOnline(uint64_t user_id, const std::string& device_id, const std::string& connection_id);
    bool setOffline(uint64_t user_id, const std::string& device_id, const std::string& connection_id);
    void refreshOnlineAsync(uint64_t user_id, std::string device_id, std::string connection_id);
    void setOfflineAsync(uint64_t user_id, std::string device_id, std::string connection_id);

private:
    void submitOnlineTask(std::function<void()> task);
    std::string devicesKey(uint64_t user_id) const;
    std::string onlineKey(uint64_t user_id, const std::string& device_id) const;
    std::string connKey(uint64_t user_id, const std::string& device_id) const;

private:
    RedisClient* redis_client_;
    std::string gateway_id_;
    int online_ttl_seconds_;
    ThreadPool online_worker_;
};

}  // namespace nebula
