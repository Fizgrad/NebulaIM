#include "common/thread/ThreadPool.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

int main() {
    nebula::ThreadPool pool(4);
    pool.start();

    std::atomic<int> counter{0};
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 16; ++i) {
        futures.emplace_back(pool.submit([&counter, i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            ++counter;
            return i * i;
        }));
    }

    int sum = 0;
    for (auto& future : futures) {
        sum += future.get();
    }

    assert(counter == 16);
    assert(sum == 1240);

    pool.stop();
    pool.stop();

    bool threw = false;
    try {
        auto future = pool.submit([]() { return 1; });
        (void)future;
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    return 0;
}
