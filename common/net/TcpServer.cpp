#include "common/net/TcpServer.h"

#include "common/log/Logger.h"
#include "common/net/Acceptor.h"
#include "common/net/EventLoop.h"
#include "common/net/EventLoopThreadPool.h"
#include "common/net/InetAddress.h"
#include "common/net/TcpConnection.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace nebula {

namespace {

InetAddress getLocalAddress(int sockfd) {
    sockaddr_in local{};
    socklen_t addrlen = sizeof(local);
    if (::getsockname(sockfd, reinterpret_cast<sockaddr*>(&local), &addrlen) < 0) {
        LOG_ERROR(std::string("getsockname failed: ") + std::strerror(errno));
    }
    return InetAddress(local);
}

}  // namespace

TcpServer::TcpServer(EventLoop* loop,
                     const InetAddress& listen_addr,
                     std::string name,
                     bool reuse_port)
    : loop_(loop),
      name_(std::move(name)),
      acceptor_(std::make_unique<Acceptor>(loop, listen_addr, reuse_port)),
      thread_pool_(std::make_unique<EventLoopThreadPool>(loop, name_)),
      started_(false),
      next_conn_id_(1) {
    acceptor_->setNewConnectionCallback([this](int sockfd, const InetAddress& peer_addr) {
        newConnection(sockfd, peer_addr);
    });
}

TcpServer::~TcpServer() {
    for (auto& item : connections_) {
        TcpConnectionPtr conn = item.second;
        item.second.reset();
        conn->getLoop()->runInLoop([conn]() { conn->connectDestroyed(); });
    }
}

void TcpServer::setThreadNum(int num_threads) {
    thread_pool_->setThreadNum(num_threads);
}

void TcpServer::setTlsContext(std::shared_ptr<TlsContext> tls_context) {
    tls_context_ = std::move(tls_context);
}

void TcpServer::setConnectionCallback(ConnectionCallback cb) {
    connection_callback_ = std::move(cb);
}

void TcpServer::setMessageCallback(MessageCallback cb) {
    message_callback_ = std::move(cb);
}

void TcpServer::setWriteCompleteCallback(WriteCompleteCallback cb) {
    write_complete_callback_ = std::move(cb);
}

void TcpServer::start() {
    if (started_) {
        return;
    }

    started_ = true;
    thread_pool_->start();
    if (!acceptor_->listening()) {
        loop_->runInLoop([this]() { acceptor_->listen(); });
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress& peer_addr) {
    loop_->assertInLoopThread();
    EventLoop* io_loop = thread_pool_->getNextLoop();
    std::string conn_name = name_ + "-conn-" + std::to_string(next_conn_id_++);
    InetAddress local_addr = getLocalAddress(sockfd);

    auto conn = std::make_shared<TcpConnection>(io_loop, conn_name, sockfd, local_addr, peer_addr, tls_context_);
    connections_[conn_name] = conn;
    conn->setConnectionCallback(connection_callback_);
    conn->setMessageCallback(message_callback_);
    conn->setWriteCompleteCallback(write_complete_callback_);
    conn->setCloseCallback([this](const TcpConnectionPtr& connection) { removeConnection(connection); });

    io_loop->runInLoop([conn]() { conn->connectEstablished(); });
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn) {
    loop_->runInLoop([this, conn]() { removeConnectionInLoop(conn); });
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn) {
    loop_->assertInLoopThread();
    connections_.erase(conn->name());
    EventLoop* io_loop = conn->getLoop();
    io_loop->queueInLoop([conn]() { conn->connectDestroyed(); });
}

}  // namespace nebula
