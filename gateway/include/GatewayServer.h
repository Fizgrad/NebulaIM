#pragma once

#include "common/gateway/ConnectionManager.h"
#include "common/gateway/GatewayOnlineManager.h"
#include "common/gateway/GatewayRouter.h"
#include "common/net/TcpServer.h"
#include "common/protocol/PacketCodec.h"

#include <mutex>
#include <string>
#include <unordered_map>

namespace nebula {

class GatewayServer {
public:
    GatewayServer(EventLoop* loop,
                  const InetAddress& listen_addr,
                  std::string gateway_id,
                  ConnectionManager* connection_manager,
                  GatewayOnlineManager* online_manager,
                  GatewayRouter* router,
                  int worker_threads,
                  int heartbeat_timeout_ms);

    void start();

private:
    void onConnection(const TcpConnectionPtr& conn);
    void onMessage(const TcpConnectionPtr& conn, Buffer* buffer);
    void checkHeartbeatTimeout();
    std::string getConnectionIdFromConnection(const TcpConnectionPtr& conn);
    void bindConnectionIdToConnection(const TcpConnectionPtr& conn, const std::string& connection_id);

private:
    EventLoop* loop_;
    TcpServer server_;
    std::string gateway_id_;
    ConnectionManager* connection_manager_;
    GatewayOnlineManager* online_manager_;
    GatewayRouter* router_;
    PacketCodec codec_;
    int heartbeat_timeout_ms_;
    std::mutex conn_id_mutex_;
    std::unordered_map<std::string, std::string> conn_name_to_id_;
};

}  // namespace nebula
