#pragma once

#include "common/gateway/ConnectionManager.h"
#include "common/gateway/GatewayOnlineManager.h"
#include "common/protocol/PacketCodec.h"
#include "gateway.grpc.pb.h"

namespace nebula {

class GatewayServiceImpl final : public proto::GatewayService::Service {
public:
    GatewayServiceImpl(ConnectionManager* connection_manager,
                       GatewayOnlineManager* online_manager,
                       PacketCodec* codec);

    grpc::Status DeliverToConnection(grpc::ServerContext* context,
                                     const proto::DeliverToConnectionRequest* request,
                                     proto::DeliverToConnectionResponse* response) override;
    grpc::Status KickUser(grpc::ServerContext* context,
                          const proto::KickUserRequest* request,
                          proto::KickUserResponse* response) override;
    grpc::Status KickConnection(grpc::ServerContext* context,
                                const proto::KickConnectionRequest* request,
                                proto::KickConnectionResponse* response) override;
    grpc::Status GetOnlineStatus(grpc::ServerContext* context,
                                  const proto::GetOnlineStatusRequest* request,
                                  proto::GetOnlineStatusResponse* response) override;

private:
    ConnectionManager* connection_manager_;
    GatewayOnlineManager* online_manager_;
    PacketCodec* codec_;
};

}  // namespace nebula
