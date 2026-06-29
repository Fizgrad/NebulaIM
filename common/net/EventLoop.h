#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace nebula {

class Channel;
class EpollPoller;
class TimerQueue;

class EventLoop {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    void loop();
    void quit();

    void runInLoop(Functor cb);
    void queueInLoop(Functor cb);

    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

    bool isInLoopThread() const;
    void assertInLoopThread() const;

    void runAt(int64_t timestamp_ms, Functor cb);
    void runAfter(int64_t delay_ms, Functor cb);
    void runEvery(int64_t interval_ms, Functor cb);

private:
    void wakeup();
    void handleWakeupRead();
    void doPendingFunctors();

private:
    bool looping_;
    std::atomic<bool> quit_;
    const std::thread::id thread_id_;

    std::unique_ptr<EpollPoller> poller_;
    std::unique_ptr<TimerQueue> timer_queue_;

    int wakeup_fd_;
    std::unique_ptr<Channel> wakeup_channel_;

    std::mutex mutex_;
    std::vector<Functor> pending_functors_;
    bool calling_pending_functors_;
};

}  // namespace nebula
