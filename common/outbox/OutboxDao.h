#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nebula {

class MySqlConnection;
class MySqlConnectionPool;

enum class OutboxStatus {
    PENDING = 0,
    PUBLISHED = 1,
    FAILED = 2,
    DEAD = 3,
    PUBLISHING = 4
};

struct OutboxEvent {
    uint64_t id = 0;
    uint64_t event_id = 0;
    std::string aggregate_type;
    uint64_t aggregate_id = 0;
    std::string topic;
    std::string event_key;
    std::string payload;
    int status = static_cast<int>(OutboxStatus::PENDING);
    int retry_count = 0;
    int64_t next_retry_at = 0;
    int64_t created_at = 0;
    int64_t updated_at = 0;
    std::string trace_id;
};

class OutboxDao {
public:
    explicit OutboxDao(MySqlConnectionPool& pool);

    bool insertEvent(MySqlConnection& conn, const OutboxEvent& event);
    std::vector<OutboxEvent> fetchPendingEvents(size_t limit, int64_t now_ms);
    std::vector<OutboxEvent> claimPendingEvents(size_t limit, int64_t now_ms, int64_t lease_ms);
    bool markPublished(uint64_t event_id);
    bool markFailed(uint64_t event_id, int retry_count, int64_t next_retry_at);
    bool markDead(uint64_t event_id);

private:
    MySqlConnectionPool& pool_;
};

}  // namespace nebula
