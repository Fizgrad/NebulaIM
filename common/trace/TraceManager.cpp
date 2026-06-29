#include "common/trace/TraceManager.h"

#include "common/log/Logger.h"
#include "common/trace/OtlpTraceExporter.h"

#include <chrono>
#include <utility>

namespace nebula {

TraceManager& TraceManager::instance() {
    static TraceManager manager;
    return manager;
}

TraceManagerConfig TraceManager::configFrom(const Config& config, const std::string& default_service_name) {
    TraceManagerConfig trace;
    trace.enabled = config.getBool("trace.enabled", false);
    trace.service_name = config.getString("trace.service_name", default_service_name);
    trace.otlp_endpoint = config.getString("trace.otlp_endpoint", trace.otlp_endpoint);
    trace.export_timeout_ms = config.getInt("trace.export_timeout_ms", trace.export_timeout_ms);
    trace.batch_size = config.getInt("trace.batch_size", trace.batch_size);
    trace.flush_interval_ms = config.getInt("trace.flush_interval_ms", trace.flush_interval_ms);
    trace.max_queue_size = static_cast<size_t>(config.getInt64("trace.max_queue_size", static_cast<int64_t>(trace.max_queue_size)));
    return trace;
}

TraceManager::~TraceManager() {
    shutdown();
}

void TraceManager::configure(TraceManagerConfig config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = std::move(config);
    if (config_.batch_size <= 0) config_.batch_size = 64;
    if (config_.flush_interval_ms <= 0) config_.flush_interval_ms = 1000;
    if (config_.max_queue_size == 0) config_.max_queue_size = 1;
    if (config_.enabled) {
        startWorkerLocked();
        LOG_INFO("trace manager enabled service_name=" + config_.service_name + " endpoint=" + config_.otlp_endpoint);
    }
}

void TraceManager::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!worker_started_) return;
        stopping_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
    worker_started_ = false;
}

void TraceManager::record(CompletedSpan span) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!config_.enabled) return;
    if (queue_.size() >= config_.max_queue_size) {
        queue_.pop_front();
        dropped_spans_.fetch_add(1, std::memory_order_relaxed);
    }
    queue_.push_back(std::move(span));
    cv_.notify_one();
}

void TraceManager::flush() {
    while (true) {
        std::vector<CompletedSpan> batch;
        TraceManagerConfig config;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.empty()) return;
            config = config_;
            batch = takeBatchLocked(static_cast<size_t>(config_.batch_size));
        }
        if (!config.enabled || batch.empty()) return;
        std::string error;
        if (!OtlpTraceExporter({config.otlp_endpoint, config.export_timeout_ms}).exportSpans(batch, &error)) {
            LOG_WARN("trace export failed: " + error);
            return;
        }
    }
}

bool TraceManager::enabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.enabled;
}

std::string TraceManager::serviceName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.service_name;
}

size_t TraceManager::droppedSpans() const {
    return dropped_spans_.load(std::memory_order_relaxed);
}

size_t TraceManager::queuedSpans() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

void TraceManager::startWorkerLocked() {
    if (worker_started_) return;
    stopping_ = false;
    worker_started_ = true;
    worker_ = std::thread([this]() { workerLoop(); });
}

void TraceManager::workerLoop() {
    while (true) {
        std::vector<CompletedSpan> batch;
        TraceManagerConfig config;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, std::chrono::milliseconds(config_.flush_interval_ms), [this]() {
                return stopping_ || queue_.size() >= static_cast<size_t>(config_.batch_size);
            });
            config = config_;
            if (!queue_.empty()) {
                batch = takeBatchLocked(static_cast<size_t>(config_.batch_size));
            }
            if (stopping_ && batch.empty()) return;
        }
        if (!config.enabled || batch.empty()) continue;
        std::string error;
        if (!OtlpTraceExporter({config.otlp_endpoint, config.export_timeout_ms}).exportSpans(batch, &error)) {
            LOG_WARN("trace export failed: " + error);
        }
    }
}

std::vector<CompletedSpan> TraceManager::takeBatchLocked(size_t max_items) {
    std::vector<CompletedSpan> batch;
    const size_t count = std::min(max_items, queue_.size());
    batch.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        batch.push_back(std::move(queue_.front()));
        queue_.pop_front();
    }
    return batch;
}

}  // namespace nebula
