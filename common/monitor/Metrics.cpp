#include "common/monitor/Metrics.h"

#include <algorithm>
#include <sstream>

namespace nebula {

void Counter::inc(double value) {
    if (value < 0) return;
    std::lock_guard<std::mutex> lock(mutex_);
    value_ += value;
}

double Counter::value() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return value_;
}

void Gauge::set(double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    value_ = value;
}

void Gauge::inc(double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    value_ += value;
}

void Gauge::dec(double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    value_ -= value;
}

double Gauge::value() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return value_;
}

Histogram::Histogram(std::vector<double> buckets) : buckets_(std::move(buckets)) {
    std::sort(buckets_.begin(), buckets_.end());
    bucket_counts_.assign(buckets_.size(), 0);
}

void Histogram::observe(double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++count_;
    sum_ += value;
    for (size_t i = 0; i < buckets_.size(); ++i) {
        if (value <= buckets_[i]) ++bucket_counts_[i];
    }
}

std::string Histogram::render(const std::string& name, const std::string& help) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    oss << "# HELP " << name << ' ' << help << '\n';
    oss << "# TYPE " << name << " histogram\n";
    for (size_t i = 0; i < buckets_.size(); ++i) {
        oss << name << "_bucket{le=\"" << buckets_[i] << "\"} " << bucket_counts_[i] << '\n';
    }
    oss << name << "_bucket{le=\"+Inf\"} " << count_ << '\n';
    oss << name << "_sum " << sum_ << '\n';
    oss << name << "_count " << count_ << '\n';
    return oss.str();
}

}  // namespace nebula
