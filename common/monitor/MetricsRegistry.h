#pragma once

#include "common/monitor/Metrics.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace nebula {

class MetricsRegistry {
public:
    static MetricsRegistry& instance();

    Counter& counter(const std::string& name, const std::string& help);
    Gauge& gauge(const std::string& name, const std::string& help);
    Histogram& histogram(const std::string& name, const std::string& help, const std::vector<double>& buckets);
    std::string renderPrometheus() const;

private:
    MetricsRegistry() = default;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<Counter>> counters_;
    std::unordered_map<std::string, std::unique_ptr<Gauge>> gauges_;
    std::unordered_map<std::string, std::unique_ptr<Histogram>> histograms_;
    std::unordered_map<std::string, std::string> helps_;
};

}  // namespace nebula
