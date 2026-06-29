#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace nebula {

class Acceptor;
class Buffer;
class EventLoop;
class EventLoopThreadPool;
class InetAddress;
class TcpConnection;

class TcpServer {
public:
    using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
    using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
    using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*)>;
    using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;

    TcpServer(EventLoop* loop,
              const InetAddress& listen_addr,
              std::string name,
              bool reuse_port = false);

    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    void setThreadNum(int num_threads);

    void setConnectionCallback(ConnectionCallback cb);
    void setMessageCallback(MessageCallback cb);
    void setWriteCompleteCallback(WriteCompleteCallback cb);

    void start();

private:
    void newConnection(int sockfd, const InetAddress& peer_addr);
    void removeConnection(const TcpConnectionPtr& conn);
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

private:
    EventLoop* loop_;
    const std::string name_;
    std::unique_ptr<Acceptor> acceptor_;
    std::unique_ptr<EventLoopThreadPool> thread_pool_;

    ConnectionCallback connection_callback_;
    MessageCallback message_callback_;
    WriteCompleteCallback write_complete_callback_;

    bool started_;
    int next_conn_id_;

    std::unordered_map<std::string, TcpConnectionPtr> connections_;
};

}  // namespace nebula
