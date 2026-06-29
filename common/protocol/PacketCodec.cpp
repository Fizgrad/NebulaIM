#include "common/protocol/PacketCodec.h"

#include "common/net/Buffer.h"

#include <stdexcept>

namespace nebula {

std::string PacketCodec::encode(const Packet& packet) const {
    if (packet.type == MessageType::UNKNOWN || !isValidMessageType(static_cast<uint16_t>(packet.type))) {
        throw std::invalid_argument("invalid message type");
    }
    if (packet.body.size() > kMaxBodyLength) {
        throw std::invalid_argument("packet body too large");
    }

    PacketHeader header;
    header.type = static_cast<uint16_t>(packet.type);
    header.sequence_id = packet.sequence_id;
    header.body_length = static_cast<uint32_t>(packet.body.size());

    std::string out = encodeHeader(header);
    out.append(packet.body);
    return out;
}

ProtocolError PacketCodec::decode(Buffer* buffer, Packet* packet) const {
    if (buffer == nullptr || packet == nullptr) {
        return ProtocolError::INTERNAL_ERROR;
    }
    if (buffer->readableBytes() < kPacketHeaderLength) {
        return ProtocolError::INCOMPLETE_PACKET;
    }

    PacketHeader header;
    if (!decodeHeader(buffer->peek(), buffer->readableBytes(), &header)) {
        return ProtocolError::INVALID_HEADER;
    }

    ProtocolError validation = validateHeader(header);
    if (validation != ProtocolError::OK) {
        buffer->retrieveAll();
        return validation;
    }

    const size_t packet_length = kPacketHeaderLength + header.body_length;
    if (buffer->readableBytes() < packet_length) {
        return ProtocolError::INCOMPLETE_PACKET;
    }

    buffer->retrieve(kPacketHeaderLength);
    packet->type = messageTypeFromUint16(header.type);
    packet->sequence_id = header.sequence_id;
    packet->body = buffer->retrieveAsString(header.body_length);
    return ProtocolError::OK;
}

ProtocolError PacketCodec::validateHeader(const PacketHeader& header) const {
    if (header.magic != kProtocolMagic) {
        return ProtocolError::INVALID_MAGIC;
    }
    if (header.version != kProtocolVersion) {
        return ProtocolError::INVALID_VERSION;
    }
    if (header.body_length > kMaxBodyLength) {
        return ProtocolError::BODY_TOO_LARGE;
    }
    if (!isValidMessageType(header.type)) {
        return ProtocolError::INVALID_MESSAGE_TYPE;
    }
    return ProtocolError::OK;
}

}  // namespace nebula
