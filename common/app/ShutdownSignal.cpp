#include "common/app/ShutdownSignal.h"

#include <chrono>
#include <utility>

namespace nebula {

namespace {

volatile std::sig_atomic_t g_shutdown_requested = 0;

}  // namespace

ShutdownSignalWatcher::ShutdownSignalWatcher(std::function<void()> callback, int poll_interval_ms)
    : stop_(false) {
    install();
    int interval = poll_interval_ms > 0 ? poll_interval_ms : 100;
    thread_ = std::thread([this, cb = std::move(callback), interval]() mutable {
        while (!stop_.load(std::memory_order_relaxed)) {
            if (requested()) {
                cb();
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
    });
}

ShutdownSignalWatcher::~ShutdownSignalWatcher() {
    stop_.store(true, std::memory_order_relaxed);
    if (thread_.joinable()) thread_.join();
}

bool ShutdownSignalWatcher::requested() {
    return g_shutdown_requested != 0;
}

void ShutdownSignalWatcher::install() {
    std::signal(SIGINT, &ShutdownSignalWatcher::handleSignal);
    std::signal(SIGTERM, &ShutdownSignalWatcher::handleSignal);
}

void ShutdownSignalWatcher::handleSignal(int) {
    g_shutdown_requested = 1;
}

}  // namespace nebula
