#include "common/monitor/MetricsRegistry.h"

#include <sstream>

namespace nebula {

MetricsRegistry& MetricsRegistry::instance() {
    static MetricsRegistry registry;
    return registry;
}

Counter& MetricsRegistry::counter(const std::string& name, const std::string& help) {
    std::lock_guard<std::mutex> lock(mutex_);
    helps_[name] = help;
    auto& ptr = counters_[name];
    if (!ptr) ptr = std::make_unique<Counter>();
    return *ptr;
}

Gauge& MetricsRegistry::gauge(const std::string& name, const std::string& help) {
    std::lock_guard<std::mutex> lock(mutex_);
    helps_[name] = help;
    auto& ptr = gauges_[name];
    if (!ptr) ptr = std::make_unique<Gauge>();
    return *ptr;
}

Histogram& MetricsRegistry::histogram(const std::string& name, const std::string& help, const std::vector<double>& buckets) {
    std::lock_guard<std::mutex> lock(mutex_);
    helps_[name] = help;
    auto& ptr = histograms_[name];
    if (!ptr) ptr = std::make_unique<Histogram>(buckets);
    return *ptr;
}

std::string MetricsRegistry::renderPrometheus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    for (const auto& item : counters_) {
        oss << "# HELP " << item.first << ' ' << helps_.at(item.first) << '\n';
        oss << "# TYPE " << item.first << " counter\n";
        oss << item.first << ' ' << item.second->value() << "\n\n";
    }
    for (const auto& item : gauges_) {
        oss << "# HELP " << item.first << ' ' << helps_.at(item.first) << '\n';
        oss << "# TYPE " << item.first << " gauge\n";
        oss << item.first << ' ' << item.second->value() << "\n\n";
    }
    for (const auto& item : histograms_) {
        oss << item.second->render(item.first, helps_.at(item.first)) << '\n';
    }
    return oss.str();
}

}  // namespace nebula
