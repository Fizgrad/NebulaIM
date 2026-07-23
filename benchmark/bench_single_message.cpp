#include "BenchArgs.h"
#include "GatewayBenchClient.h"
#include "Stats.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    std::string host = nebula::benchmark::argValue(argc, argv, "--host", "127.0.0.1");
    uint16_t port = nebula::benchmark::portArg(argc, argv, "--port", 9000);
    int requests = nebula::benchmark::intArg(argc, argv, "--requests", 1000);
    int clients = nebula::benchmark::intArg(argc, argv, "--clients", 1);
    if (clients < 1) clients = 1;
    std::string password = nebula::benchmark::argValue(argc, argv, "--password", "password123");
    std::string run = nebula::benchmark::runId();

    std::string error;
    nebula::benchmark::GatewayBenchClient receiver(host, port);
    uint64_t receiver_id = 0;
    nebula::benchmark::LoginResult receiver_login;
    std::string receiver_name = "bench_single_receiver_" + run;
    if (!receiver.connectGateway(&error) ||
        !receiver.registerUser(receiver_name, password, &receiver_id, &error) ||
        !receiver.loginUser(receiver_name, password, "bench-single-receiver-" + run, &receiver_login, &error)) {
        std::cerr << "receiver setup failed: " << error << "\n";
        return 2;
    }

    struct Sender {
        std::unique_ptr<nebula::benchmark::GatewayBenchClient> client;
        uint64_t user_id = 0;
    };
    std::vector<Sender> senders;
    for (int i = 0; i < clients; ++i) {
        auto client = std::make_unique<nebula::benchmark::GatewayBenchClient>(host, port);
        uint64_t sender_id = 0;
        nebula::benchmark::LoginResult sender_login;
        std::string username = "bench_single_sender_" + run + "_" + std::to_string(i);
        if (!client->connectGateway(&error) ||
            !client->registerUser(username, password, &sender_id, &error) ||
            !client->loginUser(username, password, "bench-single-sender-" + std::to_string(i) + "-" + run, &sender_login, &error)) {
            std::cerr << "sender setup failed: " << error << "\n";
            return 2;
        }
        senders.push_back({std::move(client), sender_id});
    }

    nebula::benchmark::Stats stats("single_message");
    stats.start();
    for (int i = 0; i < requests; ++i) {
        auto& sender = senders[static_cast<size_t>(i % senders.size())];
        uint64_t message_id = 0;
        auto started = std::chrono::steady_clock::now();
        bool ok = sender.client->sendSingleMessage(sender.user_id, receiver_id, "bench single message", static_cast<uint32_t>(100000 + i), &message_id, &error);
        double latency = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        stats.recordLatency(latency);
        if (ok && message_id != 0) stats.incSuccess();
        else stats.incFailure();
    }
    std::cout << stats.report();
    return 0;
}
