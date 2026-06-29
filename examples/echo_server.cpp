#include "common/log/Logger.h"
#include "common/net/Buffer.h"
#include "common/net/EventLoop.h"
#include "common/net/InetAddress.h"
#include "common/net/TcpConnection.h"
#include "common/net/TcpServer.h"

#include <csignal>
#include <iostream>

namespace {
nebula::EventLoop* g_loop = nullptr;

void handleSignal(int) {
    if (g_loop != nullptr) {
        g_loop->quit();
    }
}
}

int main() {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    nebula::Logger::instance().setLevel(nebula::LogLevel::INFO);

    nebula::EventLoop loop;
    g_loop = &loop;

    nebula::InetAddress listen_addr(9000);
    nebula::TcpServer server(&loop, listen_addr, "EchoServer");
    server.setThreadNum(4);

    server.setConnectionCallback([](const nebula::TcpConnection::TcpConnectionPtr& conn) {
        if (conn->connected()) {
            LOG_INFO("connection established name=" + conn->name() + " peer=" + conn->peerAddress().toIpPort());
        } else {
            LOG_INFO("connection closed name=" + conn->name());
        }
    });

    server.setMessageCallback([](const nebula::TcpConnection::TcpConnectionPtr& conn, nebula::Buffer* buffer) {
        std::string message = buffer->retrieveAllAsString();
        conn->send(message);
    });

    server.start();
    LOG_INFO("EchoServer listening on 0.0.0.0:9000");
    loop.loop();
    LOG_INFO("EchoServer stopped");
    return 0;
}
