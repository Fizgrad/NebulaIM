#pragma once

#include "common/gateway/ConnectionManager.h"
#include "common/gateway/GatewayBackendClients.h"
#include "common/gateway/GatewayOnlineManager.h"
#include "common/protocol/PacketCodec.h"
#include "common/circuitbreaker/CircuitBreakerManager.h"
#include "common/ratelimit/RateLimiter.h"

#include <functional>

namespace nebula {

class RpcExecutor;

class GatewayRouter {
public:
    using PacketSender = std::function<void(const TcpConnectionPtr&, const Packet&)>;

    GatewayRouter(ConnectionManager* connection_manager,
                  GatewayOnlineManager* online_manager,
                  GatewayBackendClients* backend_clients,
                  PacketCodec* codec,
                  PacketSender packet_sender = nullptr,
                  RpcExecutor* rpc_executor = nullptr);

    void handlePacket(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet);
    void setPacketSender(PacketSender packet_sender);
    void setRpcExecutor(RpcExecutor* rpc_executor);

private:
    void sendPacket(const TcpConnectionPtr& conn, const Packet& packet);
    void sendError(const TcpConnectionPtr& conn, uint32_t sequence_id, int code, const std::string& message, const std::string& request_id);
    bool requireAuth(const TcpConnectionPtr& conn, const std::string& connection_id, uint32_t sequence_id, std::optional<ConnectionContext>* context);

    void handleLogin(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet);
    void handleHeartbeat(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet);
    void handleSendSingleMessage(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet);
    void handleSendGroupMessage(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet);
    void handleAck(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet);
    void handlePullOfflineMessages(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet);

private:
    ConnectionManager* connection_manager_;
    GatewayOnlineManager* online_manager_;
    GatewayBackendClients* backend_clients_;
    PacketCodec* codec_;
    PacketSender packet_sender_;
    RpcExecutor* rpc_executor_;
    CircuitBreakerManager circuit_breakers_;
    RateLimiter rate_limiter_;
};

}  // namespace nebula
