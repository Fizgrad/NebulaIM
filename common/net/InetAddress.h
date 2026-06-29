#pragma once

#include <netinet/in.h>
#include <string>

namespace nebula {

class InetAddress {
public:
    explicit InetAddress(uint16_t port, const std::string& ip = "0.0.0.0");
    explicit InetAddress(const struct sockaddr_in& addr);

    const struct sockaddr_in* getSockAddr() const;
    struct sockaddr_in* getSockAddr();

    std::string toIp() const;
    uint16_t toPort() const;
    std::string toIpPort() const;

private:
    struct sockaddr_in addr_;
};

}  // namespace nebula
