#include "common/message/MessageKafkaPayload.h"

namespace nebula {

std::string MessageKafkaPayload::serializeMessageData(const proto::MessageData& data) {
    std::string payload;
    data.SerializeToString(&payload);
    return payload;
}

bool MessageKafkaPayload::deserializeMessageData(const std::string& payload, proto::MessageData* data) {
    return data != nullptr && data->ParseFromString(payload);
}

}  // namespace nebula
