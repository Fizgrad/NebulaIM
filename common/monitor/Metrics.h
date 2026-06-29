#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace nebula {

enum class MetricType {
    COUNTER,
    GAUGE,
    HISTOGRAM
};

class Counter {
public:
    void inc(double value = 1.0);
    double value() const;

private:
    mutable std::mutex mutex_;
    double value_ = 0.0;
};

class Gauge {
public:
    void set(double value);
    void inc(double value = 1.0);
    void dec(double value = 1.0);
    double value() const;

private:
    mutable std::mutex mutex_;
    double value_ = 0.0;
};

class Histogram {
public:
    explicit Histogram(std::vector<double> buckets = {1, 5, 10, 50, 100, 500, 1000});

    void observe(double value);
    std::string render(const std::string& name, const std::string& help) const;

private:
    mutable std::mutex mutex_;
    std::vector<double> buckets_;
    std::vector<uint64_t> bucket_counts_;
    uint64_t count_ = 0;
    double sum_ = 0.0;
};

}  // namespace nebula
