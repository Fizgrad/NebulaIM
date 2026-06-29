#pragma once

#include "common/gateway/ConnectionManager.h"
#include "common/protocol/PacketCodec.h"
#include "gateway.grpc.pb.h"

namespace nebula {

class GatewayServiceImpl final : public proto::GatewayService::Service {
public:
    GatewayServiceImpl(ConnectionManager* connection_manager, PacketCodec* codec, std::string gateway_id = "gateway-1");

    grpc::Status DeliverToConnection(grpc::ServerContext* context,
                                     const proto::DeliverToConnectionRequest* request,
                                     proto::DeliverToConnectionResponse* response) override;
    grpc::Status KickUser(grpc::ServerContext* context,
                          const proto::KickUserRequest* request,
                          proto::KickUserResponse* response) override;
    grpc::Status GetOnlineStatus(grpc::ServerContext* context,
                                  const proto::GetOnlineStatusRequest* request,
                                  proto::GetOnlineStatusResponse* response) override;

private:
    ConnectionManager* connection_manager_;
    PacketCodec* codec_;
    std::string gateway_id_;
};

}  // namespace nebula
