#include "BenchArgs.h"
#include "GatewayBenchClient.h"
#include "Stats.h"

#include <chrono>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::string host = nebula::benchmark::argValue(argc, argv, "--host", "127.0.0.1");
    uint16_t port = nebula::benchmark::portArg(argc, argv, "--port", 9000);
    int requests = nebula::benchmark::intArg(argc, argv, "--requests", 1000);
    std::string password = nebula::benchmark::argValue(argc, argv, "--password", "password123");
    std::string prefix = nebula::benchmark::argValue(argc, argv, "--prefix", "bench_login_" + nebula::benchmark::runId());

    nebula::benchmark::Stats stats("gateway_login");
    stats.start();
    for (int i = 0; i < requests; ++i) {
        std::string error;
        std::string username = prefix + "_" + std::to_string(i);
        nebula::benchmark::GatewayBenchClient client(host, port);
        uint64_t user_id = 0;
        if (!client.connectGateway(&error) || !client.registerUser(username, password, &user_id, &error)) {
            stats.incFailure();
            continue;
        }
        auto started = std::chrono::steady_clock::now();
        nebula::benchmark::LoginResult login;
        bool ok = client.loginUser(username, password, "bench-login-device-" + std::to_string(i), &login, &error);
        double latency = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        stats.recordLatency(latency);
        if (ok && login.user_id == user_id) {
            stats.incSuccess();
        } else {
            stats.incFailure();
        }
    }
    std::cout << stats.report();
    return 0;
}
