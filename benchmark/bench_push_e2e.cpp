#include "BenchArgs.h"
#include "GatewayBenchClient.h"
#include "Stats.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

struct Pair {
    std::unique_ptr<nebula::benchmark::GatewayBenchClient> sender;
    std::unique_ptr<nebula::benchmark::GatewayBenchClient> receiver;
    uint64_t sender_id = 0;
    uint64_t receiver_id = 0;
};

int main(int argc, char** argv) {
    std::string host = nebula::benchmark::argValue(argc, argv, "--host", "127.0.0.1");
    uint16_t port = nebula::benchmark::portArg(argc, argv, "--port", 9000);
    int requests = nebula::benchmark::intArg(argc, argv, "--requests", 100);
    int pairs = nebula::benchmark::intArg(argc, argv, "--pairs", 1);
    if (pairs < 1) pairs = 1;
    std::string password = nebula::benchmark::argValue(argc, argv, "--password", "password123");
    std::string run = nebula::benchmark::runId();

    std::string error;
    std::vector<Pair> clients;
    for (int i = 0; i < pairs; ++i) {
        Pair pair;
        pair.sender = std::make_unique<nebula::benchmark::GatewayBenchClient>(host, port);
        pair.receiver = std::make_unique<nebula::benchmark::GatewayBenchClient>(host, port);
        std::string sender_name = "bench_push_sender_" + run + "_" + std::to_string(i);
        std::string receiver_name = "bench_push_receiver_" + run + "_" + std::to_string(i);
        nebula::benchmark::LoginResult sender_login;
        nebula::benchmark::LoginResult receiver_login;
        if (!pair.sender->connectGateway(&error) ||
            !pair.receiver->connectGateway(&error) ||
            !pair.sender->registerUser(sender_name, password, &pair.sender_id, &error) ||
            !pair.receiver->registerUser(receiver_name, password, &pair.receiver_id, &error) ||
            !pair.sender->loginUser(sender_name, password, "bench-push-sender-" + std::to_string(i) + "-" + run, &sender_login, &error) ||
            !pair.receiver->loginUser(receiver_name, password, "bench-push-receiver-" + std::to_string(i) + "-" + run, &receiver_login, &error)) {
            std::cerr << "pair setup failed: " << error << "\n";
            return 2;
        }
        clients.push_back(std::move(pair));
    }

    nebula::benchmark::Stats stats("push_e2e");
    stats.start();
    for (int i = 0; i < requests; ++i) {
        auto& pair = clients[static_cast<size_t>(i % clients.size())];
        uint64_t message_id = 0;
        auto started = std::chrono::steady_clock::now();
        bool ok = pair.sender->sendSingleMessage(pair.sender_id, pair.receiver_id, "bench push e2e", static_cast<uint32_t>(200000 + i), &message_id, &error);
        nebula::proto::MessageData pushed;
        ok = ok && pair.receiver->waitPush(&pushed, 15000, &error) && pushed.message_id() == message_id;
        if (ok) pair.receiver->ackMessage(pair.receiver_id, message_id, &error);
        double latency = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        stats.recordLatency(latency);
        if (ok) stats.incSuccess();
        else stats.incFailure();
    }
    std::cout << stats.report();
    return 0;
}
