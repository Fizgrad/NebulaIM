#include "GatewayServiceImpl.h"
#include "common/gateway/ConnectionManager.h"
#include "common/protocol/PacketCodec.h"

#include <cassert>
#include <grpcpp/grpcpp.h>

int main() {
    nebula::ConnectionManager manager("gateway-test");
    nebula::PacketCodec codec;
    nebula::GatewayServiceImpl service(&manager, &codec, "gateway-test");
    grpc::ServerContext ctx;

    nebula::proto::GetOnlineStatusRequest status_req;
    status_req.set_request_id("status");
    status_req.set_user_id(10001);
    nebula::proto::GetOnlineStatusResponse status_resp;
    assert(service.GetOnlineStatus(&ctx, &status_req, &status_resp).ok());
    assert(!status_resp.online());

    auto conn_id = manager.addConnection(nullptr, "peer");
    assert(manager.bindUser(conn_id, 10001, "token"));
    nebula::proto::GetOnlineStatusResponse status_resp2;
    assert(service.GetOnlineStatus(&ctx, &status_req, &status_resp2).ok());
    assert(status_resp2.online());
    assert(status_resp2.connection_id() == conn_id);

    nebula::proto::DeliverToConnectionRequest deliver_req;
    deliver_req.set_request_id("deliver");
    deliver_req.set_user_id(10001);
    deliver_req.set_connection_id("not-exists");
    deliver_req.mutable_message()->set_message_id(1);
    nebula::proto::DeliverToConnectionResponse deliver_resp;
    assert(service.DeliverToConnection(&ctx, &deliver_req, &deliver_resp).ok());
    assert(deliver_resp.response().code() != 0);
    return 0;
}
