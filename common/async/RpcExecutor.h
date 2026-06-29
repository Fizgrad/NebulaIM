#pragma once

#include "common/net/EventLoop.h"
#include "common/thread/ThreadPool.h"

#include <atomic>
#include <exception>
#include <type_traits>
#include <utility>

namespace nebula {

class RpcExecutor {
public:
    explicit RpcExecutor(size_t thread_num);
    ~RpcExecutor();

    void start();
    void stop();

    size_t queueSize() const;
    uint64_t submittedTasks() const;
    uint64_t failedTasks() const;

    template <typename Fn, typename Callback>
    void submit(EventLoop* loop, Fn&& fn, Callback&& cb);

private:
    ThreadPool pool_;
    std::atomic<size_t> pending_;
    std::atomic<uint64_t> submitted_;
    std::atomic<uint64_t> failed_;
};

template <typename Fn, typename Callback>
void RpcExecutor::submit(EventLoop* loop, Fn&& fn, Callback&& cb) {
    using ReturnType = typename std::invoke_result<Fn>::type;
    pending_.fetch_add(1, std::memory_order_relaxed);
    submitted_.fetch_add(1, std::memory_order_relaxed);

    auto task = [this,
                 loop,
                 fn = std::forward<Fn>(fn),
                 cb = std::forward<Callback>(cb)]() mutable {
        try {
            if constexpr (std::is_void<ReturnType>::value) {
                fn();
                auto done = [cb = std::move(cb)]() mutable { cb(nullptr); };
                if (loop != nullptr) loop->queueInLoop(std::move(done));
                else done();
            } else {
                ReturnType value = fn();
                auto done = [cb = std::move(cb), value = std::move(value)]() mutable { cb(std::move(value), nullptr); };
                if (loop != nullptr) loop->queueInLoop(std::move(done));
                else done();
            }
        } catch (...) {
            failed_.fetch_add(1, std::memory_order_relaxed);
            std::exception_ptr error = std::current_exception();
            auto done = [cb = std::move(cb), error]() mutable {
                if constexpr (std::is_void<ReturnType>::value) {
                    cb(error);
                } else {
                    cb(ReturnType{}, error);
                }
            };
            if (loop != nullptr) loop->queueInLoop(std::move(done));
            else done();
        }
        pending_.fetch_sub(1, std::memory_order_relaxed);
    };

    try {
        pool_.submit(std::move(task));
    } catch (...) {
        pending_.fetch_sub(1, std::memory_order_relaxed);
        failed_.fetch_add(1, std::memory_order_relaxed);
        throw;
    }
}

}  // namespace nebula
