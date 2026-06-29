#include "common/async/RpcExecutor.h"

namespace nebula {

RpcExecutor::RpcExecutor(size_t thread_num)
    : pool_(thread_num), pending_(0), submitted_(0), failed_(0) {}

RpcExecutor::~RpcExecutor() {
    stop();
}

void RpcExecutor::start() {
    pool_.start();
}

void RpcExecutor::stop() {
    pool_.stop();
}

size_t RpcExecutor::queueSize() const {
    return pending_.load(std::memory_order_relaxed);
}

uint64_t RpcExecutor::submittedTasks() const {
    return submitted_.load(std::memory_order_relaxed);
}

uint64_t RpcExecutor::failedTasks() const {
    return failed_.load(std::memory_order_relaxed);
}

}  // namespace nebula
