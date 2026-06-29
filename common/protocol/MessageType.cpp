#include "common/protocol/MessageType.h"

namespace nebula {

std::string messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::UNKNOWN:
            return "UNKNOWN";
        case MessageType::LOGIN_REQ:
            return "LOGIN_REQ";
        case MessageType::LOGIN_RESP:
            return "LOGIN_RESP";
        case MessageType::REGISTER_REQ:
            return "REGISTER_REQ";
        case MessageType::REGISTER_RESP:
            return "REGISTER_RESP";
        case MessageType::HEARTBEAT_REQ:
            return "HEARTBEAT_REQ";
        case MessageType::HEARTBEAT_RESP:
            return "HEARTBEAT_RESP";
        case MessageType::SEND_SINGLE_MSG_REQ:
            return "SEND_SINGLE_MSG_REQ";
        case MessageType::SEND_SINGLE_MSG_RESP:
            return "SEND_SINGLE_MSG_RESP";
        case MessageType::SEND_GROUP_MSG_REQ:
            return "SEND_GROUP_MSG_REQ";
        case MessageType::SEND_GROUP_MSG_RESP:
            return "SEND_GROUP_MSG_RESP";
        case MessageType::PUSH_MSG:
            return "PUSH_MSG";
        case MessageType::ACK_REQ:
            return "ACK_REQ";
        case MessageType::ACK_RESP:
            return "ACK_RESP";
        case MessageType::PULL_OFFLINE_MSG_REQ:
            return "PULL_OFFLINE_MSG_REQ";
        case MessageType::PULL_OFFLINE_MSG_RESP:
            return "PULL_OFFLINE_MSG_RESP";
        case MessageType::ERROR_RESP:
            return "ERROR_RESP";
    }
    return "UNKNOWN";
}

bool isValidMessageType(uint16_t type) {
    return messageTypeFromUint16(type) != MessageType::UNKNOWN;
}

MessageType messageTypeFromUint16(uint16_t type) {
    switch (static_cast<MessageType>(type)) {
        case MessageType::LOGIN_REQ:
        case MessageType::LOGIN_RESP:
        case MessageType::REGISTER_REQ:
        case MessageType::REGISTER_RESP:
        case MessageType::HEARTBEAT_REQ:
        case MessageType::HEARTBEAT_RESP:
        case MessageType::SEND_SINGLE_MSG_REQ:
        case MessageType::SEND_SINGLE_MSG_RESP:
        case MessageType::SEND_GROUP_MSG_REQ:
        case MessageType::SEND_GROUP_MSG_RESP:
        case MessageType::PUSH_MSG:
        case MessageType::ACK_REQ:
        case MessageType::ACK_RESP:
        case MessageType::PULL_OFFLINE_MSG_REQ:
        case MessageType::PULL_OFFLINE_MSG_RESP:
        case MessageType::ERROR_RESP:
            return static_cast<MessageType>(type);
        case MessageType::UNKNOWN:
            return MessageType::UNKNOWN;
    }
    return MessageType::UNKNOWN;
}

}  // namespace nebula
