#include "common/net/Buffer.h"
#include "common/websocket/WebSocketCodec.h"
#include "common/websocket/WebSocketFrame.h"

#include <cassert>
#include <iostream>

int main() {
    nebula::WebSocketCodec codec;
    std::string payload = "nebula-packet-bytes";
    std::string client_frame = nebula::WebSocketFrameCodec::encode(nebula::WebSocketOpCode::BINARY, payload, true);

    nebula::Buffer buffer;
    buffer.append(client_frame);
    nebula::WebSocketFrame frame;
    assert(codec.decodeFrame(&buffer, &frame) == nebula::WebSocketCodecStatus::OK);
    assert(frame.opcode == nebula::WebSocketOpCode::BINARY);
    assert(frame.payload == payload);

    std::string server_frame = codec.encodeBinary(payload);
    size_t consumed = 0;
    nebula::WebSocketFrame decoded;
    assert(nebula::WebSocketFrameCodec::decode(server_frame.data(), server_frame.size(), false, &decoded, &consumed) == nebula::WebSocketFrameStatus::OK);
    assert(decoded.payload == payload);
    assert(consumed == server_frame.size());
    std::cout << "test_websocket_codec passed\n";
    return 0;
}
