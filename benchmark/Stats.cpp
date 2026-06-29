#include "Stats.h"

#include <algorithm>
#include <numeric>
#include <sstream>

namespace nebula::benchmark {

Stats::Stats(std::string name) : name_(std::move(name)), start_(std::chrono::steady_clock::now()) {}
void Stats::start() { start_ = std::chrono::steady_clock::now(); }
void Stats::recordLatency(double ms) { latencies_.push_back(ms); }
void Stats::incSuccess() { ++success_; }
void Stats::incFailure() { ++failure_; }

double Stats::percentile(double p) const {
    if (latencies_.empty()) return 0.0;
    auto copy = latencies_;
    std::sort(copy.begin(), copy.end());
    size_t idx = static_cast<size_t>((p / 100.0) * (copy.size() - 1));
    return copy[idx];
}

std::string Stats::report() const {
    double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
    double total = static_cast<double>(success_ + failure_);
    double avg = latencies_.empty() ? 0.0 : std::accumulate(latencies_.begin(), latencies_.end(), 0.0) / latencies_.size();
    double max = latencies_.empty() ? 0.0 : *std::max_element(latencies_.begin(), latencies_.end());
    std::ostringstream oss;
    oss << "Benchmark: " << name_ << '\n';
    oss << "Total Requests: " << static_cast<uint64_t>(total) << '\n';
    oss << "Success: " << success_ << '\n';
    oss << "Failed: " << failure_ << '\n';
    oss << "Duration: " << seconds << "s\n";
    oss << "QPS: " << (seconds > 0 ? total / seconds : 0) << '\n';
    oss << "Avg Latency: " << avg << " ms\n";
    oss << "P50: " << percentile(50) << " ms\n";
    oss << "P90: " << percentile(90) << " ms\n";
    oss << "P99: " << percentile(99) << " ms\n";
    oss << "Max: " << max << " ms\n";
    return oss.str();
}

}  // namespace nebula::benchmark
