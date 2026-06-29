#include "common/gateway/GatewayPacketHelper.h"

namespace nebula {

Packet GatewayPacketHelper::makeErrorResponse(uint32_t sequence_id, int code, const std::string& message, const std::string& request_id) {
    proto::CommonResponse resp;
    resp.set_code(code);
    resp.set_message(message);
    resp.set_request_id(request_id);
    return makeResponse(MessageType::ERROR_RESP, sequence_id, resp);
}

Packet GatewayPacketHelper::makePushMessage(uint32_t sequence_id, const proto::MessageData& message) {
    return makeResponse(MessageType::PUSH_MSG, sequence_id, message);
}

}  // namespace nebula
