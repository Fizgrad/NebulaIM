#pragma once

#include "common/protocol/PacketCodec.h"
#include "common.pb.h"

namespace nebula {

class GatewayPacketHelper {
public:
    template <typename ProtoMsg>
    static bool parseBody(const Packet& packet, ProtoMsg* msg) {
        return msg != nullptr && msg->ParseFromString(packet.body);
    }

    template <typename ProtoMsg>
    static Packet makeResponse(MessageType type, uint32_t sequence_id, const ProtoMsg& msg) {
        std::string body;
        msg.SerializeToString(&body);
        return Packet(type, sequence_id, body);
    }

    static Packet makeErrorResponse(uint32_t sequence_id, int code, const std::string& message, const std::string& request_id);
    static Packet makePushMessage(uint32_t sequence_id, const proto::MessageData& message);
};

}  // namespace nebula
