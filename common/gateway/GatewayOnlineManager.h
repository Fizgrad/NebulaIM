#pragma once

#include <cstdint>
#include <string>

namespace nebula {

class RedisClient;

class GatewayOnlineManager {
public:
    GatewayOnlineManager(RedisClient* redis_client, std::string gateway_id, int online_ttl_seconds);

    bool setOnline(uint64_t user_id, const std::string& device_id, const std::string& connection_id);
    bool refreshOnline(uint64_t user_id, const std::string& device_id, const std::string& connection_id);
    bool setOffline(uint64_t user_id, const std::string& device_id, const std::string& connection_id);

private:
    std::string devicesKey(uint64_t user_id) const;
    std::string onlineKey(uint64_t user_id, const std::string& device_id) const;
    std::string connKey(uint64_t user_id, const std::string& device_id) const;

private:
    RedisClient* redis_client_;
    std::string gateway_id_;
    int online_ttl_seconds_;
};

}  // namespace nebula
