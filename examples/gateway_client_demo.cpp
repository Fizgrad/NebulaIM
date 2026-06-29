#include "common/protocol/PacketCodec.h"
#include "user.pb.h"

#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {
std::string parseHost(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; ++i) if (std::string(argv[i]) == "--host") return argv[i + 1];
    return "127.0.0.1";
}
int parsePort(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; ++i) if (std::string(argv[i]) == "--port") return std::stoi(argv[i + 1]);
    return 9000;
}
}

int main(int argc, char** argv) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(parsePort(argc, argv)));
    inet_pton(AF_INET, parseHost(argc, argv).c_str(), &addr.sin_addr);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::cerr << "connect gateway failed" << std::endl;
        return 1;
    }
    nebula::proto::LoginRequest login;
    login.set_request_id("gateway-client-login");
    login.set_username("test");
    login.set_password("123456");
    std::string body;
    login.SerializeToString(&body);
    nebula::PacketCodec codec;
    std::string packet = codec.encode(nebula::Packet(nebula::MessageType::LOGIN_REQ, 1, body));
    ::write(fd, packet.data(), packet.size());
    char buf[4096];
    ssize_t n = ::read(fd, buf, sizeof(buf));
    std::cout << "read bytes=" << n << std::endl;
    ::close(fd);
    return 0;
}
