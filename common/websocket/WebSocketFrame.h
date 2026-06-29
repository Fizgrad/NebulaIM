#pragma once

#include <cstdint>
#include <string>

namespace nebula {

enum class WebSocketOpCode : uint8_t {
    CONTINUATION = 0x0,
    TEXT = 0x1,
    BINARY = 0x2,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA
};

enum class WebSocketFrameStatus {
    OK,
    INCOMPLETE,
    PROTOCOL_ERROR
};

struct WebSocketFrame {
    bool fin = true;
    WebSocketOpCode opcode = WebSocketOpCode::BINARY;
    bool masked = false;
    std::string payload;
};

class WebSocketFrameCodec {
public:
    static WebSocketFrameStatus decode(const char* data,
                                       size_t len,
                                       bool require_mask,
                                       WebSocketFrame* frame,
                                       size_t* consumed);

    static std::string encode(WebSocketOpCode opcode,
                              const std::string& payload,
                              bool mask_payload = false);
};

}  // namespace nebula
