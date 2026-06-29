#pragma once

#include <functional>
#include <memory>
#include <set>
#include <vector>

namespace nebula {

class Channel;
class EventLoop;
class Timer;

class TimerQueue {
public:
    using TimerCallback = std::function<void()>;

    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    TimerQueue(const TimerQueue&) = delete;
    TimerQueue& operator=(const TimerQueue&) = delete;

    void addTimer(TimerCallback cb, int64_t timestamp_ms, int64_t interval_ms);

private:
    void addTimerInLoop(std::shared_ptr<Timer> timer);
    void handleRead();
    std::vector<std::shared_ptr<Timer>> getExpired(int64_t now_ms);
    void reset(const std::vector<std::shared_ptr<Timer>>& expired, int64_t now_ms);

private:
    EventLoop* loop_;
    int timerfd_;
    std::unique_ptr<Channel> timerfd_channel_;

    std::set<std::pair<int64_t, std::shared_ptr<Timer>>> timers_;
};

}  // namespace nebula
