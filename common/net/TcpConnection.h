#pragma once

#include "common/net/Buffer.h"
#include "common/net/InetAddress.h"

#include <functional>
#include <memory>
#include <string>

namespace nebula {

class Channel;
class EventLoop;
class Socket;

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
    using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
    using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*)>;
    using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;
    using CloseCallback = std::function<void(const TcpConnectionPtr&)>;

    TcpConnection(EventLoop* loop,
                  std::string name,
                  int sockfd,
                  const InetAddress& local_addr,
                  const InetAddress& peer_addr);

    ~TcpConnection();

    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    EventLoop* getLoop() const;
    const std::string& name() const;

    const InetAddress& localAddress() const;
    const InetAddress& peerAddress() const;

    bool connected() const;

    void send(const std::string& message);
    void shutdown();
    void forceClose();

    void setConnectionCallback(ConnectionCallback cb);
    void setMessageCallback(MessageCallback cb);
    void setWriteCompleteCallback(WriteCompleteCallback cb);
    void setCloseCallback(CloseCallback cb);

    void connectEstablished();
    void connectDestroyed();

private:
    enum class State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        DISCONNECTING
    };

    void setState(State state);

    void handleRead();
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const char* data, size_t len);
    void shutdownInLoop();
    void forceCloseInLoop();

private:
    EventLoop* loop_;
    std::string name_;
    State state_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    InetAddress local_addr_;
    InetAddress peer_addr_;

    Buffer input_buffer_;
    Buffer output_buffer_;

    ConnectionCallback connection_callback_;
    MessageCallback message_callback_;
    WriteCompleteCallback write_complete_callback_;
    CloseCallback close_callback_;
};

}  // namespace nebula
