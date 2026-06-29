#pragma once

#include "common/net/InetAddress.h"

namespace nebula {

class Socket {
public:
    explicit Socket(int fd);
    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    int fd() const;

    void bindAddress(const InetAddress& local_addr);
    void listen();
    int accept(InetAddress* peer_addr);

    void shutdownWrite();

    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setTcpNoDelay(bool on);
    void setKeepAlive(bool on);

    static int createNonblockingSocket();

private:
    int fd_;
};

}  // namespace nebula
