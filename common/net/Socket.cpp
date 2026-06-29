#include "common/net/Socket.h"

#include "common/log/Logger.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace nebula {

Socket::Socket(int fd)
    : fd_(fd) {}

Socket::~Socket() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

int Socket::fd() const {
    return fd_;
}

void Socket::bindAddress(const InetAddress& local_addr) {
    if (::bind(fd_, reinterpret_cast<const sockaddr*>(local_addr.getSockAddr()), sizeof(sockaddr_in)) < 0) {
        throw std::runtime_error(std::string("bind failed: ") + std::strerror(errno));
    }
}

void Socket::listen() {
    if (::listen(fd_, SOMAXCONN) < 0) {
        throw std::runtime_error(std::string("listen failed: ") + std::strerror(errno));
    }
}

int Socket::accept(InetAddress* peer_addr) {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    int connfd = ::accept4(fd_, reinterpret_cast<sockaddr*>(&addr), &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (connfd >= 0 && peer_addr != nullptr) {
        *peer_addr = InetAddress(addr);
    }
    return connfd;
}

void Socket::shutdownWrite() {
    if (::shutdown(fd_, SHUT_WR) < 0) {
        LOG_ERROR(std::string("shutdownWrite failed: ") + std::strerror(errno));
    }
}

void Socket::setReuseAddr(bool on) {
    int opt = on ? 1 : 0;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, static_cast<socklen_t>(sizeof(opt)));
}

void Socket::setReusePort(bool on) {
#ifdef SO_REUSEPORT
    int opt = on ? 1 : 0;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &opt, static_cast<socklen_t>(sizeof(opt)));
#else
    (void)on;
#endif
}

void Socket::setTcpNoDelay(bool on) {
    int opt = on ? 1 : 0;
    ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &opt, static_cast<socklen_t>(sizeof(opt)));
}

void Socket::setKeepAlive(bool on) {
    int opt = on ? 1 : 0;
    ::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &opt, static_cast<socklen_t>(sizeof(opt)));
}

int Socket::createNonblockingSocket() {
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (fd < 0) {
        throw std::runtime_error(std::string("socket failed: ") + std::strerror(errno));
    }
    return fd;
}

}  // namespace nebula
