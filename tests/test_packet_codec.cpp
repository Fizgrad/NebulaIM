#include "common/net/Buffer.h"
#include "common/protocol/PacketCodec.h"
#include "common/protocol/PacketHeader.h"

#include <arpa/inet.h>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

std::string changeUint32(std::string packet, size_t offset, uint32_t value) {
    uint32_t network_value = htonl(value);
    std::memcpy(&packet[offset], &network_value, sizeof(network_value));
    return packet;
}

std::string changeUint16(std::string packet, size_t offset, uint16_t value) {
    uint16_t network_value = htons(value);
    std::memcpy(&packet[offset], &network_value, sizeof(network_value));
    return packet;
}

}  // namespace

int main() {
    nebula::PacketCodec codec;

    nebula::Packet login(nebula::MessageType::LOGIN_REQ, 1001, "login-body");
    std::string encoded = codec.encode(login);
    assert(encoded.size() == nebula::kPacketHeaderLength + login.body.size());

    nebula::Buffer buffer;
    buffer.append(encoded);
    nebula::Packet decoded;
    assert(codec.decode(&buffer, &decoded) == nebula::ProtocolError::OK);
    assert(decoded.type == nebula::MessageType::LOGIN_REQ);
    assert(decoded.sequence_id == 1001);
    assert(decoded.body == "login-body");
    assert(buffer.readableBytes() == 0);

    nebula::Packet empty_body(nebula::MessageType::HEARTBEAT_REQ, 1002, "");
    buffer.append(codec.encode(empty_body));
    assert(codec.decode(&buffer, &decoded) == nebula::ProtocolError::OK);
    assert(decoded.type == nebula::MessageType::HEARTBEAT_REQ);
    assert(decoded.sequence_id == 1002);
    assert(decoded.body.empty());

    std::string half_header = codec.encode(nebula::Packet(nebula::MessageType::ACK_REQ, 1003, "ack"));
    buffer.append(half_header.data(), nebula::kPacketHeaderLength - 1);
    size_t before = buffer.readableBytes();
    assert(codec.decode(&buffer, &decoded) == nebula::ProtocolError::INCOMPLETE_PACKET);
    assert(buffer.readableBytes() == before);
    buffer.retrieveAll();

    std::string half_body = codec.encode(nebula::Packet(nebula::MessageType::ACK_REQ, 1004, "ack-body"));
    buffer.append(half_body.data(), nebula::kPacketHeaderLength + 2);
    before = buffer.readableBytes();
    assert(codec.decode(&buffer, &decoded) == nebula::ProtocolError::INCOMPLETE_PACKET);
    assert(buffer.readableBytes() == before);
    buffer.append(half_body.data() + nebula::kPacketHeaderLength + 2,
                  half_body.size() - nebula::kPacketHeaderLength - 2);
    assert(codec.decode(&buffer, &decoded) == nebula::ProtocolError::OK);
    assert(decoded.sequence_id == 1004);
    assert(decoded.body == "ack-body");

    std::string p1 = codec.encode(nebula::Packet(nebula::MessageType::HEARTBEAT_REQ, 2001, "ping"));
    std::string p2 = codec.encode(nebula::Packet(nebula::MessageType::SEND_SINGLE_MSG_REQ, 2002, "hello"));
    buffer.append(p1 + p2);
    assert(codec.decode(&buffer, &decoded) == nebula::ProtocolError::OK);
    assert(decoded.sequence_id == 2001);
    assert(decoded.body == "ping");
    assert(buffer.readableBytes() == p2.size());
    assert(codec.decode(&buffer, &decoded) == nebula::ProtocolError::OK);
    assert(decoded.sequence_id == 2002);
    assert(decoded.body == "hello");
    assert(buffer.readableBytes() == 0);

    std::string invalid_magic = changeUint32(codec.encode(login), 0, 0x12345678);
    buffer.append(invalid_magic);
    assert(codec.decode(&buffer, &decoded) == nebula::ProtocolError::INVALID_MAGIC);
    assert(buffer.readableBytes() == 0);

    std::string invalid_version = changeUint16(codec.encode(login), 4, 99);
    buffer.append(invalid_version);
    assert(codec.decode(&buffer, &decoded) == nebula::ProtocolError::INVALID_VERSION);
    assert(buffer.readableBytes() == 0);

    std::string too_large = changeUint32(codec.encode(login), 12, nebula::kMaxBodyLength + 1);
    buffer.append(too_large);
    assert(codec.decode(&buffer, &decoded) == nebula::ProtocolError::BODY_TOO_LARGE);
    assert(buffer.readableBytes() == 0);

    std::string invalid_type = changeUint16(codec.encode(login), 6, 999);
    buffer.append(invalid_type);
    assert(codec.decode(&buffer, &decoded) == nebula::ProtocolError::INVALID_MESSAGE_TYPE);
    assert(buffer.readableBytes() == 0);

    bool threw = false;
    try {
        codec.encode(nebula::Packet(nebula::MessageType::UNKNOWN, 1, "bad"));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    return 0;
}
