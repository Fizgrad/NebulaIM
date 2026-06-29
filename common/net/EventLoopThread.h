#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

namespace nebula {

class EventLoop;

class EventLoopThread {
public:
    EventLoopThread();
    ~EventLoopThread();

    EventLoopThread(const EventLoopThread&) = delete;
    EventLoopThread& operator=(const EventLoopThread&) = delete;

    EventLoop* startLoop();

private:
    void threadFunc();

private:
    EventLoop* loop_;
    bool exiting_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

}  // namespace nebula
