#pragma once

#include <atomic>
#include <string>
#include <thread>

namespace nebula {

class MetricsHttpServer {
public:
    MetricsHttpServer(std::string host, int port);
    ~MetricsHttpServer();

    bool start();
    void stop();

private:
    void run();
    void handleClient(int client_fd);

private:
    std::string host_;
    int port_;
    int listen_fd_;
    std::atomic<bool> running_;
    std::thread thread_;
};

}  // namespace nebula
