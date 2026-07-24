#include "common/error/ErrorCode.h"

namespace nebula {

std::string errorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::OK: return "OK";
        case ErrorCode::UNKNOWN_ERROR: return "UNKNOWN_ERROR";
        case ErrorCode::INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case ErrorCode::CONFIG_LOAD_FAILED: return "CONFIG_LOAD_FAILED";
        case ErrorCode::INTERNAL_ERROR: return "INTERNAL_ERROR";
        case ErrorCode::NETWORK_ERROR: return "NETWORK_ERROR";
        case ErrorCode::CONNECTION_CLOSED: return "CONNECTION_CLOSED";
        case ErrorCode::PACKET_INVALID: return "PACKET_INVALID";
        case ErrorCode::PACKET_TOO_LARGE: return "PACKET_TOO_LARGE";
        case ErrorCode::AUTH_FAILED: return "AUTH_FAILED";
        case ErrorCode::TOKEN_INVALID: return "TOKEN_INVALID";
        case ErrorCode::USER_NOT_FOUND: return "USER_NOT_FOUND";
        case ErrorCode::USER_ALREADY_EXISTS: return "USER_ALREADY_EXISTS";
        case ErrorCode::PASSWORD_TOO_SHORT: return "PASSWORD_TOO_SHORT";
        case ErrorCode::USERNAME_EMPTY: return "USERNAME_EMPTY";
        case ErrorCode::PASSWORD_EMPTY: return "PASSWORD_EMPTY";
        case ErrorCode::TOKEN_EXPIRED: return "TOKEN_EXPIRED";
        case ErrorCode::DB_ERROR: return "DB_ERROR";
        case ErrorCode::REDIS_ERROR: return "REDIS_ERROR";
        case ErrorCode::KAFKA_ERROR: return "KAFKA_ERROR";
        case ErrorCode::FRIEND_ALREADY_EXISTS: return "FRIEND_ALREADY_EXISTS";
        case ErrorCode::FRIEND_NOT_FOUND: return "FRIEND_NOT_FOUND";
        case ErrorCode::CANNOT_ADD_SELF: return "CANNOT_ADD_SELF";
        case ErrorCode::GROUP_NOT_FOUND: return "GROUP_NOT_FOUND";
        case ErrorCode::GROUP_ALREADY_JOINED: return "GROUP_ALREADY_JOINED";
        case ErrorCode::GROUP_NOT_MEMBER: return "GROUP_NOT_MEMBER";
        case ErrorCode::GROUP_OWNER_CANNOT_LEAVE: return "GROUP_OWNER_CANNOT_LEAVE";
        case ErrorCode::GROUP_PERMISSION_DENIED: return "GROUP_PERMISSION_DENIED";
        case ErrorCode::MESSAGE_EMPTY: return "MESSAGE_EMPTY";
        case ErrorCode::MESSAGE_TOO_LARGE: return "MESSAGE_TOO_LARGE";
        case ErrorCode::MESSAGE_NOT_FOUND: return "MESSAGE_NOT_FOUND";
        case ErrorCode::MESSAGE_DUPLICATED: return "MESSAGE_DUPLICATED";
        case ErrorCode::MESSAGE_PERSIST_FAILED: return "MESSAGE_PERSIST_FAILED";
        case ErrorCode::MESSAGE_KAFKA_FAILED: return "MESSAGE_KAFKA_FAILED";
        case ErrorCode::MESSAGE_ACK_FAILED: return "MESSAGE_ACK_FAILED";
        case ErrorCode::MESSAGE_PERMISSION_DENIED: return "MESSAGE_PERMISSION_DENIED";
        case ErrorCode::INVALID_CONVERSATION: return "INVALID_CONVERSATION";
        case ErrorCode::PUSH_FAILED: return "PUSH_FAILED";
        case ErrorCode::PUSH_GATEWAY_NOT_FOUND: return "PUSH_GATEWAY_NOT_FOUND";
        case ErrorCode::PUSH_GATEWAY_RPC_FAILED: return "PUSH_GATEWAY_RPC_FAILED";
        case ErrorCode::PUSH_OFFLINE_SAVE_FAILED: return "PUSH_OFFLINE_SAVE_FAILED";
        case ErrorCode::PUSH_RETRY_FAILED: return "PUSH_RETRY_FAILED";
        case ErrorCode::PUSH_DLQ_FAILED: return "PUSH_DLQ_FAILED";
        case ErrorCode::PUSH_INVALID_MESSAGE: return "PUSH_INVALID_MESSAGE";
        case ErrorCode::GATEWAY_NOT_AUTHENTICATED: return "GATEWAY_NOT_AUTHENTICATED";
        case ErrorCode::GATEWAY_ALREADY_AUTHENTICATED: return "GATEWAY_ALREADY_AUTHENTICATED";
        case ErrorCode::GATEWAY_INVALID_PACKET: return "GATEWAY_INVALID_PACKET";
        case ErrorCode::GATEWAY_BACKEND_RPC_FAILED: return "GATEWAY_BACKEND_RPC_FAILED";
        case ErrorCode::GATEWAY_CONNECTION_NOT_FOUND: return "GATEWAY_CONNECTION_NOT_FOUND";
        case ErrorCode::GATEWAY_ONLINE_STATE_FAILED: return "GATEWAY_ONLINE_STATE_FAILED";
        case ErrorCode::GATEWAY_PERMISSION_DENIED: return "GATEWAY_PERMISSION_DENIED";
        case ErrorCode::GATEWAY_HEARTBEAT_TIMEOUT: return "GATEWAY_HEARTBEAT_TIMEOUT";
        case ErrorCode::RATE_LIMITED: return "RATE_LIMITED";
        case ErrorCode::SERVICE_UNAVAILABLE: return "SERVICE_UNAVAILABLE";
        case ErrorCode::CIRCUIT_OPEN: return "CIRCUIT_OPEN";
        case ErrorCode::FRIEND_REQUEST_NOT_FOUND: return "FRIEND_REQUEST_NOT_FOUND";
        case ErrorCode::FRIEND_REQUEST_ALREADY_EXISTS: return "FRIEND_REQUEST_ALREADY_EXISTS";
        case ErrorCode::FRIEND_REQUEST_ALREADY_HANDLED: return "FRIEND_REQUEST_ALREADY_HANDLED";
        case ErrorCode::CONVERSATION_NOT_FOUND: return "CONVERSATION_NOT_FOUND";
        case ErrorCode::MESSAGE_RECALL_TIMEOUT: return "MESSAGE_RECALL_TIMEOUT";
        case ErrorCode::MESSAGE_RECALL_PERMISSION_DENIED: return "MESSAGE_RECALL_PERMISSION_DENIED";
        case ErrorCode::MESSAGE_ALREADY_RECALLED: return "MESSAGE_ALREADY_RECALLED";
        case ErrorCode::DEVICE_NOT_FOUND: return "DEVICE_NOT_FOUND";
        case ErrorCode::TOKEN_REFRESH_FAILED: return "TOKEN_REFRESH_FAILED";
        case ErrorCode::OUTBOX_EVENT_NOT_FOUND: return "OUTBOX_EVENT_NOT_FOUND";
        case ErrorCode::OUTBOX_PUBLISH_FAILED: return "OUTBOX_PUBLISH_FAILED";
    }
    return "UNRECOGNIZED_ERROR_CODE";
}

}  // namespace nebula
