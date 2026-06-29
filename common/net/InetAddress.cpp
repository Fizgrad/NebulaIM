#include "common/net/InetAddress.h"

#include <arpa/inet.h>
#include <cstring>
#include <stdexcept>

namespace nebula {

InetAddress::InetAddress(uint16_t port, const std::string& ip) {
    std::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    if (::inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr) <= 0) {
        throw std::invalid_argument("invalid IPv4 address: " + ip);
    }
}

InetAddress::InetAddress(const struct sockaddr_in& addr)
    : addr_(addr) {}

const struct sockaddr_in* InetAddress::getSockAddr() const {
    return &addr_;
}

struct sockaddr_in* InetAddress::getSockAddr() {
    return &addr_;
}

std::string InetAddress::toIp() const {
    char buffer[INET_ADDRSTRLEN] = {0};
    const char* result = ::inet_ntop(AF_INET, &addr_.sin_addr, buffer, sizeof(buffer));
    return result == nullptr ? std::string() : std::string(buffer);
}

uint16_t InetAddress::toPort() const {
    return ntohs(addr_.sin_port);
}

std::string InetAddress::toIpPort() const {
    return toIp() + ":" + std::to_string(toPort());
}

}  // namespace nebula
