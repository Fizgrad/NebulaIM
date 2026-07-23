#include "common/dao/DeviceDao.h"

#include "common/db/MySqlConnection.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/db/MySqlResult.h"
#include "common/utils/TimeUtil.h"

namespace nebula {

namespace {

UserDevice parseDevice(MySqlResult& result) {
    UserDevice device;
    device.id = result.getUInt64("id");
    device.user_id = result.getUInt64("user_id");
    device.device_id = result.getString("device_id");
    device.platform = result.getString("platform");
    device.device_name = result.getString("device_name");
    device.token_hash = result.getString("token_hash");
    device.last_login_at = result.getInt64("last_login_at");
    device.last_active_at = result.getInt64("last_active_at");
    device.created_at = result.getInt64("created_at");
    device.updated_at = result.getInt64("updated_at");
    return device;
}

}  // namespace

DeviceDao::DeviceDao(MySqlConnectionPool& pool) : pool_(pool) {}

bool DeviceDao::upsertDevice(const UserDevice& device) {
    if (device.user_id == 0 || device.device_id.empty()) return false;
    auto conn = pool_.acquire();
    if (!conn) return false;
    int64_t now = TimeUtil::nowMs();
    int64_t login_at = device.last_login_at > 0 ? device.last_login_at : now;
    int64_t active_at = device.last_active_at > 0 ? device.last_active_at : login_at;
    int64_t created_at = device.created_at > 0 ? device.created_at : now;
    int64_t updated_at = device.updated_at > 0 ? device.updated_at : now;
    std::string sql = "INSERT INTO user_devices(user_id,device_id,platform,device_name,token_hash,last_login_at,last_active_at,created_at,updated_at) VALUES(" +
                      std::to_string(device.user_id) + ",'" + conn->escapeString(device.device_id) + "','" +
                      conn->escapeString(device.platform) + "','" + conn->escapeString(device.device_name) + "','" +
                      conn->escapeString(device.token_hash) + "'," + std::to_string(login_at) + "," +
                      std::to_string(active_at) + "," + std::to_string(created_at) + "," +
                      std::to_string(updated_at) +
                      ") ON DUPLICATE KEY UPDATE platform=VALUES(platform), device_name=VALUES(device_name), token_hash=VALUES(token_hash), "
                      "last_login_at=VALUES(last_login_at), last_active_at=VALUES(last_active_at), updated_at=VALUES(updated_at)";
    return conn->executeUpdate(sql);
}

std::optional<UserDevice> DeviceDao::getDevice(uint64_t user_id, const std::string& device_id) {
    if (user_id == 0 || device_id.empty()) return std::nullopt;
    auto conn = pool_.acquire();
    if (!conn) return std::nullopt;
    auto result = conn->executeQuery("SELECT * FROM user_devices WHERE user_id=" + std::to_string(user_id) +
                                     " AND device_id='" + conn->escapeString(device_id) + "' LIMIT 1");
    if (!result || !result->next()) return std::nullopt;
    return parseDevice(*result);
}

std::vector<UserDevice> DeviceDao::listDevices(uint64_t user_id) {
    std::vector<UserDevice> devices;
    if (user_id == 0) return devices;
    auto conn = pool_.acquire();
    if (!conn) return devices;
    auto result = conn->executeQuery("SELECT * FROM user_devices WHERE user_id=" + std::to_string(user_id) +
                                     " ORDER BY last_active_at DESC, last_login_at DESC");
    while (result && result->next()) devices.push_back(parseDevice(*result));
    return devices;
}

bool DeviceDao::updateLastActive(uint64_t user_id, const std::string& device_id, int64_t last_active_at) {
    if (user_id == 0 || device_id.empty()) return false;
    auto conn = pool_.acquire();
    if (!conn) return false;
    int64_t now = TimeUtil::nowMs();
    int64_t active_at = last_active_at > 0 ? last_active_at : now;
    return conn->executeUpdate("UPDATE user_devices SET last_active_at=" + std::to_string(active_at) +
                               ", updated_at=" + std::to_string(now) +
                               " WHERE user_id=" + std::to_string(user_id) +
                               " AND device_id='" + conn->escapeString(device_id) + "'");
}

bool DeviceDao::clearDeviceToken(uint64_t user_id, const std::string& device_id, int64_t updated_at) {
    if (user_id == 0 || device_id.empty()) return false;
    auto conn = pool_.acquire();
    if (!conn) return false;
    int64_t now = updated_at > 0 ? updated_at : TimeUtil::nowMs();
    return conn->executeUpdate("UPDATE user_devices SET token_hash='', last_active_at=" + std::to_string(now) +
                               ", updated_at=" + std::to_string(now) +
                               " WHERE user_id=" + std::to_string(user_id) +
                               " AND device_id='" + conn->escapeString(device_id) + "'");
}

bool DeviceDao::clearAllDeviceTokens(uint64_t user_id, int64_t updated_at) {
    if (user_id == 0) return false;
    auto conn = pool_.acquire();
    if (!conn) return false;
    int64_t now = updated_at > 0 ? updated_at : TimeUtil::nowMs();
    return conn->executeUpdate("UPDATE user_devices SET token_hash='', last_active_at=" + std::to_string(now) +
                               ", updated_at=" + std::to_string(now) +
                               " WHERE user_id=" + std::to_string(user_id));
}

bool DeviceDao::deleteDevice(uint64_t user_id, const std::string& device_id) {
    if (user_id == 0 || device_id.empty()) return false;
    auto conn = pool_.acquire();
    if (!conn) return false;
    return conn->executeUpdate("DELETE FROM user_devices WHERE user_id=" + std::to_string(user_id) +
                               " AND device_id='" + conn->escapeString(device_id) + "'");
}

bool DeviceDao::deleteDevicesByUser(uint64_t user_id) {
    if (user_id == 0) return false;
    auto conn = pool_.acquire();
    if (!conn) return false;
    return conn->executeUpdate("DELETE FROM user_devices WHERE user_id=" + std::to_string(user_id));
}

}  // namespace nebula
