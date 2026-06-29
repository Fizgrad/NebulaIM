#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nebula {

class MySqlConnectionPool;

struct OfflineMessage {
    uint64_t id = 0;
    uint64_t user_id = 0;
    uint64_t message_id = 0;
    std::string payload;
    int status = 0;
    int64_t created_at = 0;
    int64_t updated_at = 0;
};

class OfflineMessageDao {
public:
    explicit OfflineMessageDao(MySqlConnectionPool& pool);

    bool insertOfflineMessage(const OfflineMessage& message);
    std::vector<OfflineMessage> listOfflineMessages(uint64_t user_id, size_t limit);
    bool markAsDelivered(uint64_t user_id, uint64_t message_id);
    bool markBatchAsDelivered(uint64_t user_id, const std::vector<uint64_t>& message_ids);
    bool deleteDeliveredMessages(uint64_t user_id);

private:
    MySqlConnectionPool& pool_;
};

}  // namespace nebula
