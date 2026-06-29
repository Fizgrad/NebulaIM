#include "common/gateway/GatewayPacketHelper.h"
#include "common/protocol/PacketCodec.h"
#include "user.pb.h"

#include <cassert>

int main() {
    nebula::proto::LoginResponse resp;
    resp.mutable_response()->set_code(0);
    resp.mutable_response()->set_message("OK");
    resp.set_user_id(1);
    auto packet = nebula::GatewayPacketHelper::makeResponse(nebula::MessageType::LOGIN_RESP, 42, resp);
    assert(packet.sequence_id == 42);
    nebula::proto::LoginResponse parsed;
    assert(nebula::GatewayPacketHelper::parseBody(packet, &parsed));
    assert(parsed.user_id() == 1);

    auto error = nebula::GatewayPacketHelper::makeErrorResponse(43, 100, "bad", "req");
    assert(error.type == nebula::MessageType::ERROR_RESP);
    nebula::proto::CommonResponse common;
    assert(nebula::GatewayPacketHelper::parseBody(error, &common));
    assert(common.code() == 100);

    nebula::proto::MessageData msg;
    msg.set_message_id(9);
    auto push = nebula::GatewayPacketHelper::makePushMessage(0, msg);
    assert(push.type == nebula::MessageType::PUSH_MSG);
    return 0;
}
