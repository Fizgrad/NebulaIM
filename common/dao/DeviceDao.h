#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nebula {

class MySqlConnectionPool;

struct UserDevice {
    uint64_t id = 0;
    uint64_t user_id = 0;
    std::string device_id;
    std::string platform;
    std::string device_name;
    std::string token_hash;
    int64_t last_login_at = 0;
    int64_t last_active_at = 0;
    int64_t created_at = 0;
    int64_t updated_at = 0;
};

class DeviceDao {
public:
    explicit DeviceDao(MySqlConnectionPool& pool);

    bool upsertDevice(const UserDevice& device);
    std::optional<UserDevice> getDevice(uint64_t user_id, const std::string& device_id);
    std::vector<UserDevice> listDevices(uint64_t user_id);
    bool updateLastActive(uint64_t user_id, const std::string& device_id, int64_t last_active_at);
    bool clearDeviceToken(uint64_t user_id, const std::string& device_id, int64_t updated_at);
    bool clearAllDeviceTokens(uint64_t user_id, int64_t updated_at);
    bool deleteDevice(uint64_t user_id, const std::string& device_id);
    bool deleteDevicesByUser(uint64_t user_id);

private:
    MySqlConnectionPool& pool_;
};

}  // namespace nebula
