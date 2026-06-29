#pragma once

#include "common/gateway/ConnectionContext.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace nebula {

class TcpConnection;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

class ConnectionManager {
public:
    explicit ConnectionManager(std::string gateway_id = "gateway-1");

    std::string addConnection(const TcpConnectionPtr& conn, const std::string& peer_addr);
    void removeConnection(const std::string& connection_id);
    bool bindUser(const std::string& connection_id, uint64_t user_id, const std::string& token);
    bool bindUser(const std::string& connection_id, uint64_t user_id, const std::string& token, const std::string& device_id, const std::string& platform);
    std::optional<ConnectionContext> getContext(const std::string& connection_id);
    std::optional<ConnectionContext> getContextByUserId(uint64_t user_id);
    TcpConnectionPtr getConnection(const std::string& connection_id);
    TcpConnectionPtr getConnectionByUserId(uint64_t user_id);
    std::vector<TcpConnectionPtr> getConnectionsByUserId(uint64_t user_id);
    std::vector<ConnectionContext> getContextsByUserId(uint64_t user_id);
    bool updateActiveTime(const std::string& connection_id);
    std::vector<std::string> getTimeoutConnections(int64_t now_ms, int64_t timeout_ms);
    size_t connectionCount() const;
    size_t onlineUserCount() const;

private:
    std::string generateConnectionId();

private:
    std::string gateway_id_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, TcpConnectionPtr> connections_;
    std::unordered_map<std::string, ConnectionContext> contexts_;
    std::unordered_map<uint64_t, std::string> user_to_connection_;
    std::unordered_map<uint64_t, std::unordered_map<std::string, std::string>> user_device_to_connection_;
    std::atomic<uint64_t> next_conn_id_;
};

}  // namespace nebula
