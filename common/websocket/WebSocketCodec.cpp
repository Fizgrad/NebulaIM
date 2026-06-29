#include "common/websocket/WebSocketCodec.h"

#include "common/net/Buffer.h"

namespace nebula {

WebSocketCodecStatus WebSocketCodec::decodeFrame(Buffer* buffer, WebSocketFrame* frame) const {
    if (buffer == nullptr || frame == nullptr) {
        return WebSocketCodecStatus::PROTOCOL_ERROR;
    }
    size_t consumed = 0;
    WebSocketFrameStatus status = WebSocketFrameCodec::decode(buffer->peek(), buffer->readableBytes(), true, frame, &consumed);
    if (status == WebSocketFrameStatus::INCOMPLETE) {
        return WebSocketCodecStatus::INCOMPLETE;
    }
    if (status == WebSocketFrameStatus::PROTOCOL_ERROR) {
        buffer->retrieveAll();
        return WebSocketCodecStatus::PROTOCOL_ERROR;
    }
    buffer->retrieve(consumed);
    return WebSocketCodecStatus::OK;
}

std::string WebSocketCodec::encodeBinary(const std::string& packet_bytes) const {
    return WebSocketFrameCodec::encode(WebSocketOpCode::BINARY, packet_bytes, false);
}

std::string WebSocketCodec::encodeText(const std::string& text) const {
    return WebSocketFrameCodec::encode(WebSocketOpCode::TEXT, text, false);
}

std::string WebSocketCodec::encodePong(const std::string& payload) const {
    return WebSocketFrameCodec::encode(WebSocketOpCode::PONG, payload, false);
}

std::string WebSocketCodec::encodeClose(const std::string& reason) const {
    return WebSocketFrameCodec::encode(WebSocketOpCode::CLOSE, reason, false);
}

}  // namespace nebula
