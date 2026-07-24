#include "common/monitor/MetricsHttpServer.h"
#include "common/monitor/MetricsRegistry.h"

#include <arpa/inet.h>
#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

int main() {
    nebula::MetricsRegistry::instance().counter("http_test_counter_total", "http test").inc();
    std::unique_ptr<nebula::MetricsHttpServer> server;
    uint16_t port = 0;
    for (uint16_t candidate = 19100; candidate < 19200; ++candidate) {
        auto current = std::make_unique<nebula::MetricsHttpServer>("127.0.0.1", candidate);
        if (current->start()) {
            server = std::move(current);
            port = candidate;
            break;
        }
    }
    if (!server) {
        std::cout << "test_metrics_http_server skipped: no local port available\n";
        return 0;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int reset_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    assert(reset_fd >= 0);
    sockaddr_in reset_addr{};
    reset_addr.sin_family = AF_INET;
    reset_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &reset_addr.sin_addr);
    assert(::connect(reset_fd, reinterpret_cast<sockaddr*>(&reset_addr), sizeof(reset_addr)) == 0);
    std::string reset_req = "GET /metrics HTTP/1.1\r\nHost: localhost\r\n\r\n";
    assert(::write(reset_fd, reset_req.data(), reset_req.size()) == static_cast<ssize_t>(reset_req.size()));
    linger reset_linger{1, 0};
    assert(::setsockopt(reset_fd, SOL_SOCKET, SO_LINGER, &reset_linger, sizeof(reset_linger)) == 0);
    ::close(reset_fd);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    assert(::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    std::string req = "GET /metrics HTTP/1.1\r\nHost: localhost\r\n\r\n";
    assert(::write(fd, req.data(), req.size()) == static_cast<ssize_t>(req.size()));
    char buf[4096];
    ssize_t n = ::read(fd, buf, sizeof(buf));
    assert(n > 0);
    std::string resp(buf, static_cast<size_t>(n));
    assert(resp.find("http_test_counter_total") != std::string::npos);
    ::close(fd);
    server->stop();
    return 0;
}
