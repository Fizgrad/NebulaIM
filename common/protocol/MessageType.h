#pragma once

#include <cstdint>
#include <string>

namespace nebula {

enum class MessageType : uint16_t {
    UNKNOWN = 0,

    LOGIN_REQ = 1001,
    LOGIN_RESP = 1002,

    HEARTBEAT_REQ = 1101,
    HEARTBEAT_RESP = 1102,

    SEND_SINGLE_MSG_REQ = 2001,
    SEND_SINGLE_MSG_RESP = 2002,

    SEND_GROUP_MSG_REQ = 2101,
    SEND_GROUP_MSG_RESP = 2102,

    PUSH_MSG = 3001,

    ACK_REQ = 4001,
    ACK_RESP = 4002,

    PULL_OFFLINE_MSG_REQ = 5001,
    PULL_OFFLINE_MSG_RESP = 5002,

    ERROR_RESP = 9001
};

std::string messageTypeToString(MessageType type);
bool isValidMessageType(uint16_t type);
MessageType messageTypeFromUint16(uint16_t type);

}  // namespace nebula
