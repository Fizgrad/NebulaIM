#pragma once

#include <cstdint>
#include <mutex>

namespace nebula {

class MessageIdGenerator {
public:
    explicit MessageIdGenerator(uint16_t node_id);

    uint64_t nextId();

private:
    uint64_t currentMs() const;
    void waitNextMs(uint64_t last_ms);

private:
    std::mutex mutex_;
    uint16_t node_id_;
    uint64_t last_timestamp_ms_;
    uint16_t sequence_;
};

}  // namespace nebula
