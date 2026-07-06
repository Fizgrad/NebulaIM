#include "common/message/MessageIdGenerator.h"

#include <chrono>
#include <thread>

namespace nebula {

namespace {
constexpr uint64_t kEpochMs = 1767225600000ULL;
constexpr uint16_t kMaxNodeId = 1023;
constexpr uint16_t kSequenceMask = 4095;
}

MessageIdGenerator::MessageIdGenerator(uint16_t node_id)
    : node_id_(node_id > kMaxNodeId ? kMaxNodeId : node_id),
      last_timestamp_ms_(0),
      sequence_(0) {}

uint64_t MessageIdGenerator::nextId() {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = currentMs();
    if (now < kEpochMs) {
        now = kEpochMs;
    }
    if (now < last_timestamp_ms_) {
        waitNextMs(last_timestamp_ms_);
        now = currentMs();
        if (now < kEpochMs) {
            now = kEpochMs;
        }
        if (now <= last_timestamp_ms_) {
            now = last_timestamp_ms_ + 1;
        }
    }
    if (now == last_timestamp_ms_) {
        sequence_ = (sequence_ + 1) & kSequenceMask;
        if (sequence_ == 0) {
            waitNextMs(last_timestamp_ms_);
            now = currentMs();
            if (now < kEpochMs) {
                now = kEpochMs;
            }
            if (now <= last_timestamp_ms_) {
                now = last_timestamp_ms_ + 1;
            }
        }
    } else {
        sequence_ = 0;
    }
    last_timestamp_ms_ = now;
    return ((now - kEpochMs) << 22) | (static_cast<uint64_t>(node_id_) << 12) | sequence_;
}

uint64_t MessageIdGenerator::currentMs() const {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

void MessageIdGenerator::waitNextMs(uint64_t last_ms) {
    while (true) {
        uint64_t now = currentMs();
        if (now < kEpochMs || now > last_ms) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

}  // namespace nebula
