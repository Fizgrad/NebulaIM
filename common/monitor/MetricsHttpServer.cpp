#include "common/monitor/MetricsHttpServer.h"

#include "common/log/Logger.h"
#include "common/monitor/MetricsRegistry.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace nebula {

namespace {

bool sendAll(int fd, const char* data, size_t size) {
    size_t sent = 0;
    while (sent < size) {
        const ssize_t written = ::send(fd, data + sent, size - sent, MSG_NOSIGNAL);
        if (written > 0) {
            sent += static_cast<size_t>(written);
            continue;
        }
        if (written < 0 && errno == EINTR) continue;
        return false;
    }
    return true;
}

}  // namespace

MetricsHttpServer::MetricsHttpServer(std::string host, int port)
    : host_(std::move(host)), port_(port), listen_fd_(-1), running_(false) {}

MetricsHttpServer::~MetricsHttpServer() { stop(); }

bool MetricsHttpServer::start() {
    if (running_) return true;
    const int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) return false;
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port_));
    if (::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) <= 0) addr.sin_addr.s_addr = INADDR_ANY;
    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        LOG_ERROR("MetricsHttpServer bind failed");
        ::close(listen_fd);
        return false;
    }
    if (::listen(listen_fd, 16) != 0) {
        ::close(listen_fd);
        return false;
    }
    listen_fd_ = listen_fd;
    running_ = true;
    thread_ = std::thread([this]() { run(); });
    return true;
}

void MetricsHttpServer::stop() {
    if (!running_) return;
    running_ = false;
    const int listen_fd = listen_fd_.exchange(-1);
    if (listen_fd >= 0) {
        ::shutdown(listen_fd, SHUT_RDWR);
        ::close(listen_fd);
    }
    if (thread_.joinable()) thread_.join();
}

void MetricsHttpServer::run() {
    while (running_) {
        const int listen_fd = listen_fd_.load();
        if (listen_fd < 0) break;
        int fd = ::accept(listen_fd, nullptr, nullptr);
        if (fd < 0) continue;
        handleClient(fd);
        ::close(fd);
    }
}

void MetricsHttpServer::handleClient(int client_fd) {
    timeval timeout{};
    timeout.tv_sec = 2;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    char buffer[1024];
    ssize_t received = 0;
    do {
        received = ::read(client_fd, buffer, sizeof(buffer));
    } while (received < 0 && errno == EINTR);
    if (received <= 0) return;
    std::string body = MetricsRegistry::instance().renderPrometheus();
    std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain; version=0.0.4\r\nContent-Length: " +
                           std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
    if (!sendAll(client_fd, response.data(), response.size())) {
        LOG_DEBUG("MetricsHttpServer client disconnected before response completed");
    }
}

}  // namespace nebula
