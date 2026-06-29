#pragma once

#include "common/utils/NonCopyable.h"

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace nebula {

class ThreadPool : private NonCopyable {
public:
    explicit ThreadPool(size_t thread_num, size_t max_queue_size = 0);
    ~ThreadPool();

    void start();
    void stop();
    size_t queueSize() const;

    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>;

private:
    void workerLoop();

private:
    size_t thread_num_;
    size_t max_queue_size_;
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool running_;
    bool stopping_;
};

template <typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
    using ReturnType = typename std::invoke_result<F, Args...>::type;

    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<ReturnType> result = task->get_future();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_ || stopping_) {
            throw std::runtime_error("ThreadPool is not running");
        }
        if (max_queue_size_ > 0 && tasks_.size() >= max_queue_size_) {
            throw std::runtime_error("ThreadPool queue is full");
        }
        tasks_.emplace([task]() { (*task)(); });
    }

    cv_.notify_one();
    return result;
}

}  // namespace nebula
