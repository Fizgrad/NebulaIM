#include "common/net/EventLoopThreadPool.h"

#include "common/net/EventLoop.h"
#include "common/net/EventLoopThread.h"

#include <stdexcept>

namespace nebula {

EventLoopThreadPool::EventLoopThreadPool(EventLoop* base_loop, std::string name)
    : base_loop_(base_loop),
      name_(std::move(name)),
      started_(false),
      num_threads_(0),
      next_(0) {}

EventLoopThreadPool::~EventLoopThreadPool() = default;

void EventLoopThreadPool::setThreadNum(int num_threads) {
    if (started_) {
        throw std::runtime_error("cannot set thread number after EventLoopThreadPool started");
    }
    num_threads_ = num_threads < 0 ? 0 : num_threads;
}

void EventLoopThreadPool::start() {
    if (started_) {
        return;
    }

    base_loop_->assertInLoopThread();
    started_ = true;

    for (int i = 0; i < num_threads_; ++i) {
        auto thread = std::make_unique<EventLoopThread>();
        EventLoop* loop = thread->startLoop();
        threads_.push_back(std::move(thread));
        loops_.push_back(loop);
    }
}

EventLoop* EventLoopThreadPool::getNextLoop() {
    base_loop_->assertInLoopThread();

    EventLoop* loop = base_loop_;
    if (!loops_.empty()) {
        loop = loops_[static_cast<size_t>(next_)];
        ++next_;
        if (static_cast<size_t>(next_) >= loops_.size()) {
            next_ = 0;
        }
    }
    return loop;
}

}  // namespace nebula
