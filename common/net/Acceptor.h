#pragma once

#include "common/net/Socket.h"

#include <functional>
#include <memory>

namespace nebula {

class Channel;
class EventLoop;

class Acceptor {
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress& peer_addr)>;

    Acceptor(EventLoop* loop, const InetAddress& listen_addr, bool reuse_port);
    ~Acceptor();

    Acceptor(const Acceptor&) = delete;
    Acceptor& operator=(const Acceptor&) = delete;

    void setNewConnectionCallback(NewConnectionCallback cb);
    void listen();
    bool listening() const;

private:
    void handleRead();

private:
    EventLoop* loop_;
    Socket accept_socket_;
    std::unique_ptr<Channel> accept_channel_;
    NewConnectionCallback new_connection_callback_;
    bool listening_;
};

}  // namespace nebula
