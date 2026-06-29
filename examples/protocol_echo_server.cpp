#include "common/log/Logger.h"
#include "common/net/Buffer.h"
#include "common/net/EventLoop.h"
#include "common/net/InetAddress.h"
#include "common/net/TcpConnection.h"
#include "common/net/TcpServer.h"
#include "common/protocol/PacketCodec.h"

#include <csignal>

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

    nebula::PacketCodec codec;
    nebula::InetAddress listen_addr(9001);
    nebula::TcpServer server(&loop, listen_addr, "ProtocolEchoServer");
    server.setThreadNum(4);

    server.setConnectionCallback([](const nebula::TcpConnection::TcpConnectionPtr& conn) {
        if (conn->connected()) {
            LOG_INFO("protocol connection established name=" + conn->name() + " peer=" + conn->peerAddress().toIpPort());
        } else {
            LOG_INFO("protocol connection closed name=" + conn->name());
        }
    });

    server.setMessageCallback([&codec](const nebula::TcpConnection::TcpConnectionPtr& conn, nebula::Buffer* buffer) {
        while (buffer->readableBytes() >= nebula::kPacketHeaderLength) {
            nebula::Packet packet;
            nebula::ProtocolError err = codec.decode(buffer, &packet);
            if (err == nebula::ProtocolError::OK) {
                LOG_INFO("protocol packet type=" + nebula::messageTypeToString(packet.type) +
                         " sequence_id=" + std::to_string(packet.sequence_id) +
                         " body=" + packet.body);
                conn->send(codec.encode(packet));
            } else if (err == nebula::ProtocolError::INCOMPLETE_PACKET) {
                break;
            } else {
                LOG_ERROR("protocol decode failed: " + nebula::protocolErrorToString(err));
                conn->forceClose();
                break;
            }
        }
    });

    server.start();
    LOG_INFO("ProtocolEchoServer listening on 0.0.0.0:9001");
    loop.loop();
    LOG_INFO("ProtocolEchoServer stopped");
    return 0;
}
