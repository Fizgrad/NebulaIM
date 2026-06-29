#pragma once

#include "common/config/Config.h"
#include "common/trace/TraceSpan.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace nebula {

struct TraceManagerConfig {
    bool enabled = false;
    std::string service_name = "nebulaim";
    std::string otlp_endpoint = "http://127.0.0.1:4318/v1/traces";
    int export_timeout_ms = 2000;
    int batch_size = 64;
    int flush_interval_ms = 1000;
    size_t max_queue_size = 4096;
};

class TraceManager {
public:
    static TraceManager& instance();
    static TraceManagerConfig configFrom(const Config& config, const std::string& default_service_name);

    void configure(TraceManagerConfig config);
    void shutdown();
    void record(CompletedSpan span);
    void flush();

    bool enabled() const;
    std::string serviceName() const;
    size_t droppedSpans() const;
    size_t queuedSpans() const;

private:
    TraceManager() = default;
    ~TraceManager();

    void startWorkerLocked();
    void workerLoop();
    std::vector<CompletedSpan> takeBatchLocked(size_t max_items);

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    TraceManagerConfig config_;
    std::deque<CompletedSpan> queue_;
    std::thread worker_;
    bool stopping_ = false;
    bool worker_started_ = false;
    std::atomic<size_t> dropped_spans_{0};
};

}  // namespace nebula
