#include "common/websocket/WebSocketHandshake.h"

#include <cassert>
#include <iostream>

int main() {
    std::string request =
        "GET /ws HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";
    auto result = nebula::WebSocketHandshake::buildServerResponse(request);
    assert(result.ok);
    assert(result.response.find("HTTP/1.1 101 Switching Protocols") != std::string::npos);
    assert(result.response.find("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != std::string::npos);

    auto bad = nebula::WebSocketHandshake::buildServerResponse("POST / HTTP/1.1\r\n\r\n");
    assert(!bad.ok);
    assert(bad.response.find("400 Bad Request") != std::string::npos);
    std::cout << "test_websocket_handshake passed\n";
    return 0;
}
