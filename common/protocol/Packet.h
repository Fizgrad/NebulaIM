#pragma once

#include "common/protocol/MessageType.h"

#include <cstdint>
#include <string>

namespace nebula {

struct Packet {
    MessageType type = MessageType::UNKNOWN;
    uint32_t sequence_id = 0;
    std::string body;

    Packet() = default;
    Packet(MessageType type_, uint32_t sequence_id_, std::string body_);
};

}  // namespace nebula
