#include "common/trace/TraceId.h"

#include <atomic>
#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>

namespace nebula {

std::string TraceId::generate() {
    static std::atomic<uint64_t> counter{0};
    static thread_local std::mt19937_64 rng(std::random_device{}());
    uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::system_clock::now().time_since_epoch()).count());
    uint64_t rnd = rng();
    uint64_t seq = counter.fetch_add(1, std::memory_order_relaxed);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(16) << now
        << std::setw(16) << (rnd ^ seq);
    return oss.str();
}

std::string TraceId::generateSpanId() {
    static std::atomic<uint64_t> counter{0};
    static thread_local std::mt19937_64 rng(std::random_device{}());
    uint64_t rnd = rng() ^ counter.fetch_add(1, std::memory_order_relaxed);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << rnd;
    return oss.str();
}

bool TraceId::isValid(const std::string& trace_id) {
    if (trace_id.size() < 16 || trace_id.size() > 64) return false;
    for (char c : trace_id) {
        const bool digit = c >= '0' && c <= '9';
        const bool lower = c >= 'a' && c <= 'f';
        const bool upper = c >= 'A' && c <= 'F';
        if (!digit && !lower && !upper && c != '-') return false;
    }
    return true;
}

bool TraceId::isValidSpanId(const std::string& span_id) {
    if (span_id.size() != 16) return false;
    for (char c : span_id) {
        const bool digit = c >= '0' && c <= '9';
        const bool lower = c >= 'a' && c <= 'f';
        const bool upper = c >= 'A' && c <= 'F';
        if (!digit && !lower && !upper) return false;
    }
    return true;
}

}  // namespace nebula
