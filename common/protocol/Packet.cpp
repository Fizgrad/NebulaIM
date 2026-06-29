#include "common/protocol/Packet.h"

#include <utility>

namespace nebula {

Packet::Packet(MessageType type_, uint32_t sequence_id_, std::string body_)
    : type(type_),
      sequence_id(sequence_id_),
      body(std::move(body_)) {}

}  // namespace nebula
