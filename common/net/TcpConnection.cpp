#include "common/net/TcpConnection.h"

#include "common/log/Logger.h"
#include "common/net/Channel.h"
#include "common/net/EventLoop.h"
#include "common/net/Socket.h"

#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

namespace nebula {

TcpConnection::TcpConnection(EventLoop* loop,
                             std::string name,
                             int sockfd,
                             const InetAddress& local_addr,
                             const InetAddress& peer_addr)
    : loop_(loop),
      name_(std::move(name)),
      state_(State::CONNECTING),
      socket_(std::make_unique<Socket>(sockfd)),
      channel_(std::make_unique<Channel>(loop, sockfd)),
      local_addr_(local_addr),
      peer_addr_(peer_addr) {
    socket_->setKeepAlive(true);
    channel_->setReadCallback([this]() { handleRead(); });
    channel_->setWriteCallback([this]() { handleWrite(); });
    channel_->setCloseCallback([this]() { handleClose(); });
    channel_->setErrorCallback([this]() { handleError(); });
}

TcpConnection::~TcpConnection() = default;

EventLoop* TcpConnection::getLoop() const {
    return loop_;
}

const std::string& TcpConnection::name() const {
    return name_;
}

const InetAddress& TcpConnection::localAddress() const {
    return local_addr_;
}

const InetAddress& TcpConnection::peerAddress() const {
    return peer_addr_;
}

bool TcpConnection::connected() const {
    return state_ == State::CONNECTED;
}

void TcpConnection::send(const std::string& message) {
    if (connected()) {
        loop_->runInLoop([self = shared_from_this(), message]() {
            self->sendInLoop(message.data(), message.size());
        });
    }
}

void TcpConnection::shutdown() {
    if (state_ == State::CONNECTED) {
        setState(State::DISCONNECTING);
        loop_->runInLoop([self = shared_from_this()]() { self->shutdownInLoop(); });
    }
}

void TcpConnection::forceClose() {
    if (state_ == State::CONNECTED || state_ == State::DISCONNECTING) {
        setState(State::DISCONNECTING);
        loop_->queueInLoop([self = shared_from_this()]() { self->forceCloseInLoop(); });
    }
}

void TcpConnection::setConnectionCallback(ConnectionCallback cb) {
    connection_callback_ = std::move(cb);
}

void TcpConnection::setMessageCallback(MessageCallback cb) {
    message_callback_ = std::move(cb);
}

void TcpConnection::setWriteCompleteCallback(WriteCompleteCallback cb) {
    write_complete_callback_ = std::move(cb);
}

void TcpConnection::setCloseCallback(CloseCallback cb) {
    close_callback_ = std::move(cb);
}

void TcpConnection::connectEstablished() {
    loop_->assertInLoopThread();
    setState(State::CONNECTED);
    channel_->enableReading();
    if (connection_callback_) {
        connection_callback_(shared_from_this());
    }
}

void TcpConnection::connectDestroyed() {
    loop_->assertInLoopThread();
    if (state_ == State::CONNECTED) {
        setState(State::DISCONNECTED);
        channel_->disableAll();
        if (connection_callback_) {
            connection_callback_(shared_from_this());
        }
    }
    channel_->remove();
}

void TcpConnection::setState(State state) {
    state_ = state;
}

void TcpConnection::handleRead() {
    loop_->assertInLoopThread();
    int saved_errno = 0;
    ssize_t n = input_buffer_.readFd(channel_->fd(), &saved_errno);
    if (n > 0) {
        if (message_callback_) {
            message_callback_(shared_from_this(), &input_buffer_);
        }
    } else if (n == 0) {
        handleClose();
    } else {
        errno = saved_errno;
        handleError();
    }
}

void TcpConnection::handleWrite() {
    loop_->assertInLoopThread();
    if (!channel_->isWriting()) {
        return;
    }

    int saved_errno = 0;
    ssize_t n = output_buffer_.writeFd(channel_->fd(), &saved_errno);
    if (n > 0) {
        if (output_buffer_.readableBytes() == 0) {
            channel_->disableWriting();
            if (write_complete_callback_) {
                loop_->queueInLoop([self = shared_from_this()]() {
                    if (self->write_complete_callback_) {
                        self->write_complete_callback_(self);
                    }
                });
            }
            if (state_ == State::DISCONNECTING) {
                shutdownInLoop();
            }
        }
    } else if (saved_errno != EAGAIN) {
        LOG_ERROR(std::string("connection write failed: ") + std::strerror(saved_errno));
    }
}

void TcpConnection::handleClose() {
    loop_->assertInLoopThread();
    setState(State::DISCONNECTED);
    channel_->disableAll();
    auto self = shared_from_this();
    if (connection_callback_) {
        connection_callback_(self);
    }
    if (close_callback_) {
        close_callback_(self);
    }
}

void TcpConnection::handleError() {
    int optval = 0;
    socklen_t optlen = sizeof(optval);
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
        optval = errno;
    }
    LOG_ERROR("TcpConnection error name=" + name_ + " error=" + std::strerror(optval));
}

void TcpConnection::sendInLoop(const char* data, size_t len) {
    loop_->assertInLoopThread();
    if (state_ == State::DISCONNECTED) {
        return;
    }

    ssize_t nwrote = 0;
    size_t remaining = len;
    bool fault_error = false;

    if (!channel_->isWriting() && output_buffer_.readableBytes() == 0) {
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0) {
            remaining = len - static_cast<size_t>(nwrote);
            if (remaining == 0 && write_complete_callback_) {
                loop_->queueInLoop([self = shared_from_this()]() {
                    if (self->write_complete_callback_) {
                        self->write_complete_callback_(self);
                    }
                });
            }
        } else {
            nwrote = 0;
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                fault_error = true;
                LOG_ERROR(std::string("sendInLoop write failed: ") + std::strerror(errno));
            }
        }
    }

    if (!fault_error && remaining > 0) {
        output_buffer_.append(data + nwrote, remaining);
        if (!channel_->isWriting()) {
            channel_->enableWriting();
        }
    }
}

void TcpConnection::shutdownInLoop() {
    loop_->assertInLoopThread();
    if (!channel_->isWriting()) {
        socket_->shutdownWrite();
    }
}

void TcpConnection::forceCloseInLoop() {
    loop_->assertInLoopThread();
    if (state_ == State::CONNECTED || state_ == State::DISCONNECTING) {
        handleClose();
    }
}

}  // namespace nebula
