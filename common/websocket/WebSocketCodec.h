#pragma once

#include "common/websocket/WebSocketFrame.h"

#include <string>

namespace nebula {

class Buffer;

enum class WebSocketCodecStatus {
    OK,
    INCOMPLETE,
    PROTOCOL_ERROR
};

class WebSocketCodec {
public:
    WebSocketCodecStatus decodeFrame(Buffer* buffer, WebSocketFrame* frame) const;

    std::string encodeBinary(const std::string& packet_bytes) const;
    std::string encodeText(const std::string& text) const;
    std::string encodePong(const std::string& payload) const;
    std::string encodeClose(const std::string& reason = "") const;
};

}  // namespace nebula
