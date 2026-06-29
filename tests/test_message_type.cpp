#include "common/protocol/MessageType.h"

#include <cassert>

int main() {
    assert(nebula::messageTypeToString(nebula::MessageType::LOGIN_REQ) == "LOGIN_REQ");
    assert(nebula::messageTypeToString(nebula::MessageType::REGISTER_REQ) == "REGISTER_REQ");
    assert(nebula::messageTypeToString(nebula::MessageType::UNKNOWN) == "UNKNOWN");
    assert(nebula::isValidMessageType(static_cast<uint16_t>(nebula::MessageType::LOGIN_REQ)));
    assert(nebula::isValidMessageType(static_cast<uint16_t>(nebula::MessageType::REGISTER_REQ)));
    assert(!nebula::isValidMessageType(999));
    assert(nebula::messageTypeFromUint16(1001) == nebula::MessageType::LOGIN_REQ);
    assert(nebula::messageTypeFromUint16(1003) == nebula::MessageType::REGISTER_REQ);
    assert(nebula::messageTypeFromUint16(999) == nebula::MessageType::UNKNOWN);
    assert(nebula::messageTypeFromUint16(0) == nebula::MessageType::UNKNOWN);
    return 0;
}
