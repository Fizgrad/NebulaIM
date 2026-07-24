#include "common/net/Buffer.h"
#include "common/protocol/PacketCodec.h"
#include "common/protocol/ProtocolError.h"
#include "common/websocket/WebSocketFrame.h"
#include "user.pb.h"

#include <arpa/inet.h>
#include <ctime>
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

bool readPacket(int fd, nebula::PacketCodec* packet_codec, nebula::Packet* packet) {
    char buf[4096];
    ssize_t n = ::read(fd, buf, sizeof(buf));
    if (n <= 0) return false;

    nebula::WebSocketFrame frame;
    size_t consumed = 0;
    auto frame_status = nebula::WebSocketFrameCodec::decode(buf, static_cast<size_t>(n), false, &frame, &consumed);
    if (frame_status != nebula::WebSocketFrameStatus::OK || frame.opcode != nebula::WebSocketOpCode::BINARY) {
        return false;
    }

    nebula::Buffer packet_buffer;
    packet_buffer.append(frame.payload);
    return packet_codec->decode(&packet_buffer, packet) == nebula::ProtocolError::OK;
}

void writePacket(int fd, nebula::PacketCodec* packet_codec, nebula::MessageType type, uint32_t sequence_id, const std::string& body) {
    std::string packet = packet_codec->encode(nebula::Packet(type, sequence_id, body));
    std::string frame = nebula::WebSocketFrameCodec::encode(nebula::WebSocketOpCode::BINARY, packet, true);
    ::write(fd, frame.data(), frame.size());
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
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"  // gitleaks:allow RFC 6455 example nonce
        "Sec-WebSocket-Version: 13\r\n\r\n";
    ::write(fd, handshake.data(), handshake.size());
    char buf[4096];
    ssize_t n = ::read(fd, buf, sizeof(buf));
    if (n <= 0 || std::string(buf, static_cast<size_t>(n)).find("101 Switching Protocols") == std::string::npos) {
        std::cerr << "websocket handshake failed\n";
        ::close(fd);
        return 1;
    }

    std::string username = "websocket_demo_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(::getpid());
    std::string password = "password123";
    nebula::PacketCodec packet_codec;

    nebula::proto::RegisterRequest reg;
    reg.set_request_id("gateway-websocket-register");
    reg.set_username(username);
    reg.set_password(password);
    reg.set_nickname("WebSocket Demo");
    std::string body;
    reg.SerializeToString(&body);
    writePacket(fd, &packet_codec, nebula::MessageType::REGISTER_REQ, 1, body);

    nebula::Packet response_packet;
    if (!readPacket(fd, &packet_codec, &response_packet)) {
        std::cerr << "register response read failed\n";
        ::close(fd);
        return 1;
    }
    nebula::proto::RegisterResponse register_resp;
    if (!register_resp.ParseFromString(response_packet.body)) {
        std::cerr << "register response parse failed\n";
        ::close(fd);
        return 1;
    }
    std::cout << "Register code=" << register_resp.response().code() << " user_id=" << register_resp.user_id() << std::endl;
    if (register_resp.response().code() != 0) {
        ::close(fd);
        return 1;
    }

    nebula::proto::LoginRequest login;
    login.set_request_id("gateway-websocket-login");
    login.set_username(username);
    login.set_password(password);
    login.set_device_id("web-demo");
    login.set_platform("web");
    body.clear();
    login.SerializeToString(&body);
    writePacket(fd, &packet_codec, nebula::MessageType::LOGIN_REQ, 2, body);

    if (!readPacket(fd, &packet_codec, &response_packet)) {
        std::cerr << "login response read failed\n";
        ::close(fd);
        return 1;
    }
    nebula::proto::LoginResponse login_resp;
    if (!login_resp.ParseFromString(response_packet.body)) {
        std::cerr << "login response parse failed\n";
        ::close(fd);
        return 1;
    }
    std::cout << "Login code=" << login_resp.response().code()
              << " user_id=" << login_resp.user_id() << std::endl;
    ::close(fd);
    return login_resp.response().code() == 0 ? 0 : 1;
}
