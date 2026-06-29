#include "common/protocol/PacketCodec.h"
#include "common/websocket/WebSocketFrame.h"
#include "user.pb.h"

#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {

struct UrlParts {
    std::string host = "127.0.0.1";
    int port = 9000;
    std::string path = "/";
};

UrlParts parseUrl(int argc, char** argv) {
    std::string url = "ws://127.0.0.1:9000/";
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--url") url = argv[i + 1];
    }
    const std::string prefix = "ws://";
    if (url.rfind(prefix, 0) == 0) url = url.substr(prefix.size());
    UrlParts parts;
    auto slash = url.find('/');
    std::string host_port = slash == std::string::npos ? url : url.substr(0, slash);
    parts.path = slash == std::string::npos ? "/" : url.substr(slash);
    auto colon = host_port.find(':');
    if (colon == std::string::npos) {
        parts.host = host_port;
    } else {
        parts.host = host_port.substr(0, colon);
        parts.port = std::stoi(host_port.substr(colon + 1));
    }
    return parts;
}

}  // namespace

int main(int argc, char** argv) {
    UrlParts url = parseUrl(argc, argv);
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cerr << "socket failed\n";
        return 1;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(url.port));
    inet_pton(AF_INET, url.host.c_str(), &addr.sin_addr);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::cerr << "connect gateway failed\n";
        ::close(fd);
        return 1;
    }

    std::string handshake =
        "GET " + url.path + " HTTP/1.1\r\n"
        "Host: " + url.host + "\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";
    ::write(fd, handshake.data(), handshake.size());
    char buf[4096];
    ssize_t n = ::read(fd, buf, sizeof(buf));
    if (n <= 0 || std::string(buf, static_cast<size_t>(n)).find("101 Switching Protocols") == std::string::npos) {
        std::cerr << "websocket handshake failed\n";
        ::close(fd);
        return 1;
    }

    nebula::proto::LoginRequest login;
    login.set_request_id("gateway-websocket-login");
    login.set_username("test");
    login.set_password("123456");
    login.set_device_id("web-demo");
    login.set_platform("web");
    std::string body;
    login.SerializeToString(&body);

    nebula::PacketCodec packet_codec;
    std::string packet = packet_codec.encode(nebula::Packet(nebula::MessageType::LOGIN_REQ, 1, body));
    std::string frame = nebula::WebSocketFrameCodec::encode(nebula::WebSocketOpCode::BINARY, packet, true);
    ::write(fd, frame.data(), frame.size());
    n = ::read(fd, buf, sizeof(buf));
    std::cout << "read websocket bytes=" << n << std::endl;
    ::close(fd);
    return 0;
}
