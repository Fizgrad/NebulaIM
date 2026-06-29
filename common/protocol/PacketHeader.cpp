#include "common/protocol/PacketHeader.h"

#include <arpa/inet.h>
#include <cstring>

namespace nebula {

namespace {

void appendUint16(std::string* out, uint16_t value) {
    uint16_t network_value = htons(value);
    out->append(reinterpret_cast<const char*>(&network_value), sizeof(network_value));
}

void appendUint32(std::string* out, uint32_t value) {
    uint32_t network_value = htonl(value);
    out->append(reinterpret_cast<const char*>(&network_value), sizeof(network_value));
}

uint16_t readUint16(const char* data) {
    uint16_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    return ntohs(value);
}

uint32_t readUint32(const char* data) {
    uint32_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    return ntohl(value);
}

}  // namespace

std::string encodeHeader(const PacketHeader& header) {
    std::string out;
    out.reserve(kPacketHeaderLength);
    appendUint32(&out, header.magic);
    appendUint16(&out, header.version);
    appendUint16(&out, header.type);
    appendUint32(&out, header.sequence_id);
    appendUint32(&out, header.body_length);
    return out;
}

bool decodeHeader(const char* data, size_t len, PacketHeader* header) {
    if (data == nullptr || header == nullptr || len < kPacketHeaderLength) {
        return false;
    }

    header->magic = readUint32(data);
    header->version = readUint16(data + 4);
    header->type = readUint16(data + 6);
    header->sequence_id = readUint32(data + 8);
    header->body_length = readUint32(data + 12);
    return true;
}

}  // namespace nebula
