#include "common/protocol/ProtocolError.h"

namespace nebula {

std::string protocolErrorToString(ProtocolError error) {
    switch (error) {
        case ProtocolError::OK:
            return "OK";
        case ProtocolError::INCOMPLETE_PACKET:
            return "INCOMPLETE_PACKET";
        case ProtocolError::INVALID_MAGIC:
            return "INVALID_MAGIC";
        case ProtocolError::INVALID_VERSION:
            return "INVALID_VERSION";
        case ProtocolError::INVALID_MESSAGE_TYPE:
            return "INVALID_MESSAGE_TYPE";
        case ProtocolError::BODY_TOO_LARGE:
            return "BODY_TOO_LARGE";
        case ProtocolError::INVALID_HEADER:
            return "INVALID_HEADER";
        case ProtocolError::INTERNAL_ERROR:
            return "INTERNAL_ERROR";
    }
    return "UNKNOWN_PROTOCOL_ERROR";
}

}  // namespace nebula
