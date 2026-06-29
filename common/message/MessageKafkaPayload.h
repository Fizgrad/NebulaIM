#pragma once

#include "common.pb.h"

#include <string>

namespace nebula {

class MessageKafkaPayload {
public:
    static std::string serializeMessageData(const proto::MessageData& data);
    static bool deserializeMessageData(const std::string& payload, proto::MessageData* data);
};

}  // namespace nebula
