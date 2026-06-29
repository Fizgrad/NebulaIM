#include "common/net/Acceptor.h"

#include "common/log/Logger.h"
#include "common/net/Channel.h"
#include "common/net/EventLoop.h"

#include <cerrno>
#include <cstring>
#include <unistd.h>

namespace nebula {

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listen_addr, bool reuse_port)
    : loop_(loop),
      accept_socket_(Socket::createNonblockingSocket()),
      accept_channel_(std::make_unique<Channel>(loop, accept_socket_.fd())),
      listening_(false) {
    accept_socket_.setReuseAddr(true);
    accept_socket_.setReusePort(reuse_port);
    accept_socket_.bindAddress(listen_addr);
    accept_channel_->setReadCallback([this]() { handleRead(); });
}

Acceptor::~Acceptor() {
    accept_channel_->disableAll();
    accept_channel_->remove();
}

void Acceptor::setNewConnectionCallback(NewConnectionCallback cb) {
    new_connection_callback_ = std::move(cb);
}

void Acceptor::listen() {
    loop_->assertInLoopThread();
    listening_ = true;
    accept_socket_.listen();
    accept_channel_->enableReading();
}

bool Acceptor::listening() const {
    return listening_;
}

void Acceptor::handleRead() {
    loop_->assertInLoopThread();
    InetAddress peer_addr(0);
    int connfd = accept_socket_.accept(&peer_addr);
    if (connfd >= 0) {
        if (new_connection_callback_) {
            new_connection_callback_(connfd, peer_addr);
        } else {
            ::close(connfd);
        }
        return;
    }

    if (errno == EMFILE) {
        LOG_ERROR("accept failed: process file descriptor limit reached");
    } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
        LOG_ERROR(std::string("accept failed: ") + std::strerror(errno));
    }
}

}  // namespace nebula
