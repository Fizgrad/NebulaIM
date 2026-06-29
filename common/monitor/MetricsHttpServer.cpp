#include "common/monitor/MetricsHttpServer.h"

#include "common/log/Logger.h"
#include "common/monitor/MetricsRegistry.h"

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace nebula {

MetricsHttpServer::MetricsHttpServer(std::string host, int port)
    : host_(std::move(host)), port_(port), listen_fd_(-1), running_(false) {}

MetricsHttpServer::~MetricsHttpServer() { stop(); }

bool MetricsHttpServer::start() {
    if (running_) return true;
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) return false;
    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port_));
    if (::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) <= 0) addr.sin_addr.s_addr = INADDR_ANY;
    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        LOG_ERROR("MetricsHttpServer bind failed");
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }
    if (::listen(listen_fd_, 16) != 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }
    running_ = true;
    thread_ = std::thread([this]() { run(); });
    return true;
}

void MetricsHttpServer::stop() {
    if (!running_) return;
    running_ = false;
    if (listen_fd_ >= 0) {
        ::shutdown(listen_fd_, SHUT_RDWR);
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
    if (thread_.joinable()) thread_.join();
}

void MetricsHttpServer::run() {
    while (running_) {
        int fd = ::accept(listen_fd_, nullptr, nullptr);
        if (fd < 0) continue;
        handleClient(fd);
        ::close(fd);
    }
}

void MetricsHttpServer::handleClient(int client_fd) {
    char buffer[1024];
    ::read(client_fd, buffer, sizeof(buffer));
    std::string body = MetricsRegistry::instance().renderPrometheus();
    std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain; version=0.0.4\r\nContent-Length: " +
                           std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
    ::write(client_fd, response.data(), response.size());
}

}  // namespace nebula
