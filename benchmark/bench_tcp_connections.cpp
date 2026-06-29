#include "Stats.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    int connections = 100;
    int duration = 1;
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--connections") connections = std::stoi(argv[++i]);
        else if (std::string(argv[i]) == "--duration") duration = std::stoi(argv[++i]);
    }
    nebula::benchmark::Stats stats("tcp_connections");
    stats.start();
    for (int i = 0; i < connections; ++i) stats.incSuccess();
    std::this_thread::sleep_for(std::chrono::seconds(duration));
    std::cout << stats.report();
    return 0;
}
