#pragma once

#include <memory>
#include <string>
#include <vector>

namespace nebula {

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool {
public:
    EventLoopThreadPool(EventLoop* base_loop, std::string name);
    ~EventLoopThreadPool();

    EventLoopThreadPool(const EventLoopThreadPool&) = delete;
    EventLoopThreadPool& operator=(const EventLoopThreadPool&) = delete;

    void setThreadNum(int num_threads);
    void start();

    EventLoop* getNextLoop();

private:
    EventLoop* base_loop_;
    std::string name_;
    bool started_;
    int num_threads_;
    int next_;

    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;
};

}  // namespace nebula
