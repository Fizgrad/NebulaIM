#include "common/protocol/ProtocolError.h"

#include <cassert>

int main() {
    assert(nebula::protocolErrorToString(nebula::ProtocolError::OK) == "OK");
    assert(nebula::protocolErrorToString(nebula::ProtocolError::INCOMPLETE_PACKET) == "INCOMPLETE_PACKET");
    assert(nebula::protocolErrorToString(nebula::ProtocolError::INVALID_MAGIC) == "INVALID_MAGIC");
    assert(nebula::protocolErrorToString(nebula::ProtocolError::INVALID_VERSION) == "INVALID_VERSION");
    assert(nebula::protocolErrorToString(nebula::ProtocolError::INVALID_MESSAGE_TYPE) == "INVALID_MESSAGE_TYPE");
    assert(nebula::protocolErrorToString(nebula::ProtocolError::BODY_TOO_LARGE) == "BODY_TOO_LARGE");
    assert(nebula::protocolErrorToString(nebula::ProtocolError::INVALID_HEADER) == "INVALID_HEADER");
    assert(nebula::protocolErrorToString(nebula::ProtocolError::INTERNAL_ERROR) == "INTERNAL_ERROR");
    assert(nebula::protocolErrorToString(static_cast<nebula::ProtocolError>(999)) == "UNKNOWN_PROTOCOL_ERROR");
    return 0;
}
