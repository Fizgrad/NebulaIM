#pragma once

#include "common/protocol/Packet.h"
#include "common/protocol/PacketHeader.h"
#include "common/protocol/ProtocolError.h"

#include <string>

namespace nebula {

class Buffer;

class PacketCodec {
public:
    PacketCodec() = default;

    std::string encode(const Packet& packet) const;
    ProtocolError decode(Buffer* buffer, Packet* packet) const;

private:
    ProtocolError validateHeader(const PacketHeader& header) const;
};

}  // namespace nebula
