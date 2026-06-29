#include "common/outbox/OutboxDao.h"

#include "common/db/MySqlConnection.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/db/MySqlResult.h"
#include "common/db/MySqlTransaction.h"
#include "common/utils/TimeUtil.h"

namespace nebula {

namespace {

OutboxEvent parseOutboxEvent(MySqlResult& result) {
    OutboxEvent event;
    event.id = result.getUInt64("id");
    event.event_id = result.getUInt64("event_id");
    event.aggregate_type = result.getString("aggregate_type");
    event.aggregate_id = result.getUInt64("aggregate_id");
    event.topic = result.getString("topic");
    event.event_key = result.getString("event_key");
    event.payload = result.getString("payload");
    event.status = result.getInt("status");
    event.retry_count = result.getInt("retry_count");
    event.next_retry_at = result.getInt64("next_retry_at");
    event.created_at = result.getInt64("created_at");
    event.updated_at = result.getInt64("updated_at");
    event.trace_id = result.getString("trace_id");
    return event;
}

}  // namespace

OutboxDao::OutboxDao(MySqlConnectionPool& pool) : pool_(pool) {}

bool OutboxDao::insertEvent(MySqlConnection& conn, const OutboxEvent& event) {
    int64_t now = event.created_at > 0 ? event.created_at : TimeUtil::nowMs();
    std::string sql = "INSERT INTO outbox_events(event_id,aggregate_type,aggregate_id,topic,event_key,payload,status,retry_count,next_retry_at,created_at,updated_at,trace_id) VALUES(" +
                      std::to_string(event.event_id) + ",'" + conn.escapeString(event.aggregate_type) + "'," +
                      std::to_string(event.aggregate_id) + ",'" + conn.escapeString(event.topic) + "','" +
                      conn.escapeString(event.event_key) + "','" + conn.escapeString(event.payload) + "'," +
                      std::to_string(event.status) + "," + std::to_string(event.retry_count) + "," +
                      std::to_string(event.next_retry_at) + "," + std::to_string(now) + "," +
                      std::to_string(event.updated_at > 0 ? event.updated_at : now) + ",'" +
                      conn.escapeString(event.trace_id) + "')";
    return conn.executeUpdate(sql);
}

std::vector<OutboxEvent> OutboxDao::fetchPendingEvents(size_t limit, int64_t now_ms) {
    std::vector<OutboxEvent> events;
    auto conn = pool_.acquire();
    if (!conn) return events;
    std::string sql = "SELECT * FROM outbox_events WHERE status IN (0,2) AND next_retry_at<=" +
                      std::to_string(now_ms) + " ORDER BY created_at ASC, id ASC LIMIT " + std::to_string(limit);
    auto result = conn->executeQuery(sql);
    while (result && result->next()) {
        events.push_back(parseOutboxEvent(*result));
    }
    return events;
}

std::vector<OutboxEvent> OutboxDao::claimPendingEvents(size_t limit, int64_t now_ms, int64_t lease_ms) {
    std::vector<OutboxEvent> events;
    auto conn = pool_.acquire();
    if (!conn) return events;
    MySqlTransaction tx(*conn);
    if (!tx.active()) return events;
    std::string sql = "SELECT * FROM outbox_events WHERE status IN (0,2,4) AND next_retry_at<=" +
                      std::to_string(now_ms) + " ORDER BY created_at ASC, id ASC LIMIT " +
                      std::to_string(limit) + " FOR UPDATE SKIP LOCKED";
    auto result = conn->executeQuery(sql);
    while (result && result->next()) events.push_back(parseOutboxEvent(*result));
    int64_t lease_until = now_ms + (lease_ms > 0 ? lease_ms : 30000);
    for (const auto& event : events) {
        if (!conn->executeUpdate("UPDATE outbox_events SET status=4, next_retry_at=" + std::to_string(lease_until) +
                                 ", updated_at=" + std::to_string(now_ms) +
                                 " WHERE event_id=" + std::to_string(event.event_id))) {
            events.clear();
            return events;
        }
    }
    if (!tx.commit()) events.clear();
    return events;
}

bool OutboxDao::markPublished(uint64_t event_id) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    return conn->executeUpdate("UPDATE outbox_events SET status=1, updated_at=" + std::to_string(TimeUtil::nowMs()) +
                               " WHERE event_id=" + std::to_string(event_id));
}

bool OutboxDao::markFailed(uint64_t event_id, int retry_count, int64_t next_retry_at) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    return conn->executeUpdate("UPDATE outbox_events SET status=2, retry_count=" + std::to_string(retry_count) +
                               ", next_retry_at=" + std::to_string(next_retry_at) +
                               ", updated_at=" + std::to_string(TimeUtil::nowMs()) +
                               " WHERE event_id=" + std::to_string(event_id));
}

bool OutboxDao::markDead(uint64_t event_id) {
    auto conn = pool_.acquire();
    if (!conn) return false;
    return conn->executeUpdate("UPDATE outbox_events SET status=3, updated_at=" + std::to_string(TimeUtil::nowMs()) +
                               " WHERE event_id=" + std::to_string(event_id));
}

}  // namespace nebula
