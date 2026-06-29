#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nebula {

class RedisClient;

struct OnlineStatus {
    bool online = false;
    std::string gateway_id;
    std::string connection_id;
    std::string device_id;
};

class OnlineStatusManager {
public:
    explicit OnlineStatusManager(RedisClient* redis_client);

    OnlineStatus getOnlineStatus(uint64_t user_id);
    std::vector<OnlineStatus> getOnlineStatuses(uint64_t user_id);
    bool setOnline(uint64_t user_id, const std::string& device_id, const std::string& gateway_id, const std::string& connection_id, int ttl_seconds);
    bool setOffline(uint64_t user_id);

private:
    std::string devicesKey(uint64_t user_id) const;
    std::string onlineKey(uint64_t user_id, const std::string& device_id) const;
    std::string connKey(uint64_t user_id, const std::string& device_id) const;

private:
    RedisClient* redis_client_;
};

}  // namespace nebula
