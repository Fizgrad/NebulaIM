#pragma once

#include <atomic>
#include <csignal>
#include <functional>
#include <thread>

namespace nebula {

class ShutdownSignalWatcher {
public:
    explicit ShutdownSignalWatcher(std::function<void()> callback, int poll_interval_ms = 100);
    ~ShutdownSignalWatcher();

    ShutdownSignalWatcher(const ShutdownSignalWatcher&) = delete;
    ShutdownSignalWatcher& operator=(const ShutdownSignalWatcher&) = delete;

    static bool requested();

private:
    static void install();
    static void handleSignal(int);

    std::atomic<bool> stop_;
    std::thread thread_;
};

}  // namespace nebula
