#pragma once

#include <string>

namespace nebula {

enum class ProtocolError {
    OK = 0,
    INCOMPLETE_PACKET,
    INVALID_MAGIC,
    INVALID_VERSION,
    INVALID_MESSAGE_TYPE,
    BODY_TOO_LARGE,
    INVALID_HEADER,
    INTERNAL_ERROR
};

std::string protocolErrorToString(ProtocolError error);

}  // namespace nebula
