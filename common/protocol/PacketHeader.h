#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace nebula {

constexpr uint32_t kProtocolMagic = 0x4E494D42;
constexpr uint16_t kProtocolVersion = 1;
constexpr uint32_t kMaxBodyLength = 1024 * 1024;
constexpr size_t kPacketHeaderLength = 16;

struct PacketHeader {
    uint32_t magic = kProtocolMagic;
    uint16_t version = kProtocolVersion;
    uint16_t type = 0;
    uint32_t sequence_id = 0;
    uint32_t body_length = 0;
};

std::string encodeHeader(const PacketHeader& header);
bool decodeHeader(const char* data, size_t len, PacketHeader* header);

}  // namespace nebula
