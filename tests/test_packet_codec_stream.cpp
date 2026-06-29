#include "common/net/Buffer.h"
#include "common/protocol/PacketCodec.h"

#include <cassert>
#include <string>
#include <vector>

int main() {
    const std::string first = nebula::PacketCodec::encode(nebula::MessageType::HEARTBEAT_REQ, 11, "ping");
    const std::string second = nebula::PacketCodec::encode(nebula::MessageType::SEND_SINGLE_MSG_REQ, 12, "hello");

    nebula::Buffer buffer;
    buffer.append(first.data(), 5);

    nebula::PacketDecodeError error = nebula::PacketDecodeError::NONE;
    auto packet = nebula::PacketCodec::tryDecode(&buffer, &error);
    assert(!packet.has_value());
    assert(error == nebula::PacketDecodeError::NEED_MORE_DATA);
    assert(buffer.readableBytes() == 5);

    buffer.append(first.data() + 5, first.size() - 5);
    packet = nebula::PacketCodec::tryDecode(&buffer, &error);
    assert(packet.has_value());
    assert(error == nebula::PacketDecodeError::NONE);
    assert(packet->type == nebula::MessageType::HEARTBEAT_REQ);
    assert(packet->sequence_id == 11);
    assert(packet->body == "ping");
    assert(buffer.readableBytes() == 0);

    buffer.append(first + second);
    std::vector<nebula::Packet> packets;
    while (true) {
        auto decoded = nebula::PacketCodec::tryDecode(&buffer, &error);
        if (!decoded.has_value()) {
            assert(error == nebula::PacketDecodeError::NEED_MORE_DATA);
            break;
        }
        packets.push_back(*decoded);
    }

    assert(packets.size() == 2);
    assert(packets[0].sequence_id == 11);
    assert(packets[0].body == "ping");
    assert(packets[1].sequence_id == 12);
    assert(packets[1].body == "hello");
    assert(buffer.readableBytes() == 0);

    const std::string partial_body = nebula::PacketCodec::encode(nebula::MessageType::ACK_REQ, 13, "ack-body");
    buffer.append(partial_body.data(), nebula::kPacketHeaderLength + 2);
    packet = nebula::PacketCodec::tryDecode(&buffer, &error);
    assert(!packet.has_value());
    assert(error == nebula::PacketDecodeError::NEED_MORE_DATA);
    assert(buffer.readableBytes() == nebula::kPacketHeaderLength + 2);

    buffer.append(partial_body.data() + nebula::kPacketHeaderLength + 2,
                  partial_body.size() - nebula::kPacketHeaderLength - 2);
    packet = nebula::PacketCodec::tryDecode(&buffer, &error);
    assert(packet.has_value());
    assert(packet->sequence_id == 13);
    assert(packet->body == "ack-body");

    return 0;
}
