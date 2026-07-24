#include "common/net/TcpConnection.h"

#include "common/log/Logger.h"
#include "common/net/Channel.h"
#include "common/net/EventLoop.h"
#include "common/net/Socket.h"
#include "common/tls/TlsContext.h"

#include <cerrno>
#include <cstring>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace nebula {

TcpConnection::TcpConnection(EventLoop* loop,
                             std::string name,
                             int sockfd,
                             const InetAddress& local_addr,
                             const InetAddress& peer_addr,
                             std::shared_ptr<TlsContext> tls_context)
    : loop_(loop),
      name_(std::move(name)),
      state_(State::CONNECTING),
      socket_(std::make_unique<Socket>(sockfd)),
      channel_(std::make_unique<Channel>(loop, sockfd)),
      tls_context_(std::move(tls_context)),
      local_addr_(local_addr),
      peer_addr_(peer_addr) {
    socket_->setKeepAlive(true);
    channel_->setReadCallback([this]() { handleRead(); });
    channel_->setWriteCallback([this]() { handleWrite(); });
    channel_->setCloseCallback([this]() { handleClose(); });
    channel_->setErrorCallback([this]() { handleError(); });
}

TcpConnection::~TcpConnection() {
    if (ssl_ != nullptr) {
        SSL_free(ssl_);
        ssl_ = nullptr;
    }
}

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
    if (tls_context_ && tls_context_->enabled()) {
        ssl_ = tls_context_->newSsl(channel_->fd());
        if (ssl_ == nullptr) {
            LOG_ERROR("failed to create TLS session for " + name_);
            forceCloseInLoop();
            return;
        }
    }
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
    if (ssl_ != nullptr) {
        handleTlsRead();
        return;
    }
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
    if (ssl_ != nullptr) {
        handleTlsWrite();
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
    if (ssl_ != nullptr) {
        output_buffer_.append(data, len);
        if (!channel_->isWriting()) {
            channel_->enableWriting();
        }
        handleTlsWrite();
        return;
    }

    ssize_t nwrote = 0;
    size_t remaining = len;
    bool fault_error = false;

    if (!channel_->isWriting() && output_buffer_.readableBytes() == 0) {
        nwrote = ::send(channel_->fd(), data, len, MSG_NOSIGNAL);
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

bool TcpConnection::doTlsHandshake() {
    if (ssl_ == nullptr || tls_handshake_done_) return true;
    int rc = SSL_accept(ssl_);
    if (rc == 1) {
        tls_handshake_done_ = true;
        if (output_buffer_.readableBytes() == 0 && channel_->isWriting()) {
            channel_->disableWriting();
        }
        LOG_INFO("TLS handshake completed for " + name_);
        return true;
    }
    int err = SSL_get_error(ssl_, rc);
    if (err == SSL_ERROR_WANT_READ) {
        if (channel_->isWriting()) {
            channel_->disableWriting();
        }
        return false;
    }
    if (err == SSL_ERROR_WANT_WRITE) {
        if (!channel_->isWriting()) {
            channel_->enableWriting();
        }
        return false;
    }
    char buffer[256];
    ERR_error_string_n(ERR_get_error(), buffer, sizeof(buffer));
    LOG_ERROR("TLS handshake failed for " + name_ + ": " + buffer);
    handleClose();
    return false;
}

void TcpConnection::handleTlsRead() {
    if (!doTlsHandshake()) return;

    bool got_data = false;
    char buffer[16384];
    while (true) {
        int n = SSL_read(ssl_, buffer, sizeof(buffer));
        if (n > 0) {
            input_buffer_.append(buffer, static_cast<size_t>(n));
            got_data = true;
            continue;
        }
        int err = SSL_get_error(ssl_, n);
        if (err == SSL_ERROR_WANT_READ) {
            break;
        }
        if (err == SSL_ERROR_WANT_WRITE) {
            if (!channel_->isWriting()) channel_->enableWriting();
            break;
        }
        if (err == SSL_ERROR_ZERO_RETURN) {
            handleClose();
            return;
        }
        char errbuf[256];
        ERR_error_string_n(ERR_get_error(), errbuf, sizeof(errbuf));
        LOG_ERROR("TLS read failed for " + name_ + ": " + errbuf);
        handleClose();
        return;
    }
    if (got_data && message_callback_) {
        message_callback_(shared_from_this(), &input_buffer_);
    }
    if (output_buffer_.readableBytes() > 0 && !channel_->isWriting()) {
        channel_->enableWriting();
    }
}

void TcpConnection::handleTlsWrite() {
    if (!doTlsHandshake()) return;

    while (output_buffer_.readableBytes() > 0) {
        int n = SSL_write(ssl_, output_buffer_.peek(), static_cast<int>(output_buffer_.readableBytes()));
        if (n > 0) {
            output_buffer_.retrieve(static_cast<size_t>(n));
            continue;
        }
        int err = SSL_get_error(ssl_, n);
        if (err == SSL_ERROR_WANT_WRITE) {
            return;
        }
        if (err == SSL_ERROR_WANT_READ) {
            return;
        }
        char errbuf[256];
        ERR_error_string_n(ERR_get_error(), errbuf, sizeof(errbuf));
        LOG_ERROR("TLS write failed for " + name_ + ": " + errbuf);
        handleClose();
        return;
    }

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
}

void TcpConnection::shutdownInLoop() {
    loop_->assertInLoopThread();
    if (!channel_->isWriting()) {
        if (ssl_ != nullptr && tls_handshake_done_) {
            SSL_shutdown(ssl_);
        }
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
