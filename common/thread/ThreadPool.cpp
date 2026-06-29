#include "common/thread/ThreadPool.h"

namespace nebula {

ThreadPool::ThreadPool(size_t thread_num)
    : thread_num_(thread_num),
      running_(false),
      stopping_(false) {
    if (thread_num_ == 0) {
        thread_num_ = 1;
    }
}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        return;
    }

    running_ = true;
    stopping_ = false;
    workers_.reserve(thread_num_);
    for (size_t i = 0; i < thread_num_; ++i) {
        workers_.emplace_back([this]() { workerLoop(); });
    }
}

void ThreadPool::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_ && stopping_) {
            return;
        }
        stopping_ = true;
    }

    cv_.notify_all();

    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    workers_.clear();

    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
}

void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() { return stopping_ || !tasks_.empty(); });
            if (stopping_ && tasks_.empty()) {
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}

}  // namespace nebula
