#include "common/gateway/ConnectionManager.h"

#include "common/utils/TimeUtil.h"

namespace nebula {

ConnectionManager::ConnectionManager(std::string gateway_id)
    : gateway_id_(std::move(gateway_id)), next_conn_id_(1) {}

std::string ConnectionManager::addConnection(const TcpConnectionPtr& conn, const std::string& peer_addr) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = generateConnectionId();
    int64_t now = TimeUtil::nowMs();
    ConnectionContext ctx;
    ctx.connection_id = id;
    ctx.connected_at_ms = now;
    ctx.last_active_ms = now;
    ctx.peer_addr = peer_addr;
    connections_[id] = conn;
    contexts_[id] = ctx;
    return id;
}

void ConnectionManager::removeConnection(const std::string& connection_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = contexts_.find(connection_id);
    if (it != contexts_.end() && it->second.user_id != 0) {
        auto dit = user_device_to_connection_.find(it->second.user_id);
        if (dit != user_device_to_connection_.end()) {
            dit->second.erase(it->second.device_id);
            if (dit->second.empty()) user_device_to_connection_.erase(dit);
        }
    }
    contexts_.erase(connection_id);
    connections_.erase(connection_id);
}

bool ConnectionManager::bindUser(const std::string& connection_id,
                                 uint64_t user_id,
                                 const std::string& token,
                                 const std::string& device_id,
                                 const std::string& platform) {
    if (user_id == 0 || device_id.empty()) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = contexts_.find(connection_id);
    if (it == contexts_.end()) return false;
    it->second.user_id = user_id;
    it->second.token = token;
    it->second.device_id = device_id;
    it->second.platform = platform;
    it->second.authenticated = true;
    it->second.last_active_ms = TimeUtil::nowMs();
    user_device_to_connection_[user_id][it->second.device_id] = connection_id;
    return true;
}

bool ConnectionManager::markWebSocket(const std::string& connection_id, bool websocket) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = contexts_.find(connection_id);
    if (it == contexts_.end()) return false;
    it->second.websocket = websocket;
    return true;
}

std::optional<ConnectionContext> ConnectionManager::getContext(const std::string& connection_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = contexts_.find(connection_id);
    if (it == contexts_.end()) return std::nullopt;
    return it->second;
}

TcpConnectionPtr ConnectionManager::getConnection(const std::string& connection_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = connections_.find(connection_id);
    return it == connections_.end() ? nullptr : it->second;
}

std::vector<TcpConnectionPtr> ConnectionManager::getConnectionsByUserId(uint64_t user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TcpConnectionPtr> result;
    auto dit = user_device_to_connection_.find(user_id);
    if (dit == user_device_to_connection_.end()) return result;
    for (const auto& item : dit->second) {
        auto it = connections_.find(item.second);
        if (it != connections_.end()) result.push_back(it->second);
    }
    return result;
}

std::vector<ConnectionContext> ConnectionManager::getContextsByUserId(uint64_t user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ConnectionContext> result;
    auto dit = user_device_to_connection_.find(user_id);
    if (dit == user_device_to_connection_.end()) return result;
    for (const auto& item : dit->second) {
        auto it = contexts_.find(item.second);
        if (it != contexts_.end()) result.push_back(it->second);
    }
    return result;
}

bool ConnectionManager::updateActiveTime(const std::string& connection_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = contexts_.find(connection_id);
    if (it == contexts_.end()) return false;
    it->second.last_active_ms = TimeUtil::nowMs();
    return true;
}

std::vector<std::string> ConnectionManager::getTimeoutConnections(int64_t now_ms, int64_t timeout_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> ids;
    for (const auto& item : contexts_) {
        if (now_ms - item.second.last_active_ms > timeout_ms) ids.push_back(item.first);
    }
    return ids;
}

size_t ConnectionManager::connectionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connections_.size();
}

size_t ConnectionManager::onlineUserCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return user_device_to_connection_.size();
}

std::string ConnectionManager::generateConnectionId() {
    return gateway_id_ + "-conn-" + std::to_string(next_conn_id_++);
}

}  // namespace nebula
