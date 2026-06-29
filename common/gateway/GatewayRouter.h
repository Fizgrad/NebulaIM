#pragma once

#include "common/gateway/ConnectionManager.h"
#include "common/gateway/GatewayBackendClients.h"
#include "common/gateway/GatewayOnlineManager.h"
#include "common/protocol/PacketCodec.h"

namespace nebula {

class GatewayRouter {
public:
    GatewayRouter(ConnectionManager* connection_manager,
                  GatewayOnlineManager* online_manager,
                  GatewayBackendClients* backend_clients,
                  PacketCodec* codec);

    void handlePacket(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet);

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
};

}  // namespace nebula
