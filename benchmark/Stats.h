#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace nebula::benchmark {

class Stats {
public:
    explicit Stats(std::string name);

    void start();
    void recordLatency(double ms);
    void incSuccess();
    void incFailure();
    std::string report() const;

private:
    double percentile(double p) const;

private:
    std::string name_;
    std::chrono::steady_clock::time_point start_;
    std::vector<double> latencies_;
    uint64_t success_ = 0;
    uint64_t failure_ = 0;
};

}  // namespace nebula::benchmark
