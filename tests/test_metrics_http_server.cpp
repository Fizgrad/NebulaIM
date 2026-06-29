#include "common/monitor/MetricsHttpServer.h"
#include "common/monitor/MetricsRegistry.h"

#include <arpa/inet.h>
#include <cassert>
#include <chrono>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

int main() {
    nebula::MetricsRegistry::instance().counter("http_test_counter_total", "http test").inc();
    nebula::MetricsHttpServer server("127.0.0.1", 19100);
    assert(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(19100);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    assert(::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    std::string req = "GET /metrics HTTP/1.1\r\nHost: localhost\r\n\r\n";
    ::write(fd, req.data(), req.size());
    char buf[4096];
    ssize_t n = ::read(fd, buf, sizeof(buf));
    assert(n > 0);
    std::string resp(buf, static_cast<size_t>(n));
    assert(resp.find("http_test_counter_total") != std::string::npos);
    ::close(fd);
    server.stop();
    return 0;
}
