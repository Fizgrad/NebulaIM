#include "BenchArgs.h"
#include "Stats.h"

#include <arpa/inet.h>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <string>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>

namespace {

int connectTcp(const std::string& host, uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    int rc = ::getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result);
    if (rc != 0 || result == nullptr) {
        ::close(fd);
        return -1;
    }
    bool connected = false;
    for (addrinfo* item = result; item != nullptr; item = item->ai_next) {
        if (::connect(fd, item->ai_addr, item->ai_addrlen) == 0) {
            connected = true;
            break;
        }
    }
    ::freeaddrinfo(result);
    if (!connected) {
        ::close(fd);
        return -1;
    }
    return fd;
}

}  // namespace

int main(int argc, char** argv) {
    std::string host = nebula::benchmark::argValue(argc, argv, "--host", "127.0.0.1");
    uint16_t port = nebula::benchmark::portArg(argc, argv, "--port", 9000);
    int connections = nebula::benchmark::intArg(argc, argv, "--connections", 100);
    int rate = nebula::benchmark::intArg(argc, argv, "--rate", 0);
    int duration = nebula::benchmark::intArg(argc, argv, "--duration", 10);

    nebula::benchmark::Stats stats("tcp_connections");
    std::vector<int> fds;
    fds.reserve(static_cast<size_t>(connections));
    stats.start();
    for (int i = 0; i < connections; ++i) {
        auto started = std::chrono::steady_clock::now();
        int fd = connectTcp(host, port);
        double latency = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        stats.recordLatency(latency);
        if (fd >= 0) {
            fds.push_back(fd);
            stats.incSuccess();
        } else {
            stats.incFailure();
        }
        if (rate > 0) std::this_thread::sleep_for(std::chrono::microseconds(1000000 / rate));
    }
    std::this_thread::sleep_for(std::chrono::seconds(duration));
    for (int fd : fds) ::close(fd);
    std::cout << stats.report();
    return 0;
}
