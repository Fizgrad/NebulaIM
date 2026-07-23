#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nebula {

struct KafkaConsumerConfig {
    std::string brokers = "127.0.0.1:9092";
    std::string group_id = "nebula-consumer";
    std::string client_id = "nebula-consumer-client";
    std::string auto_offset_reset = "earliest";
    bool enable_auto_commit = false;
    int session_timeout_ms = 6000;
    int heartbeat_interval_ms = 2000;
    int max_poll_interval_ms = 300000;
    int fetch_wait_max_ms = 50;
};

struct KafkaMessage {
    std::string topic;
    std::string key;
    std::string payload;
    int partition = 0;
    int64_t offset = 0;
    bool valid = false;
};

struct KafkaLagRecord {
    std::string topic;
    std::string consumer_group;
    int partition = 0;
    int64_t committed_offset = 0;
    int64_t high_offset = 0;
    int64_t lag = 0;
};

class KafkaConsumer {
public:
    KafkaConsumer();
    ~KafkaConsumer();

    KafkaConsumer(const KafkaConsumer&) = delete;
    KafkaConsumer& operator=(const KafkaConsumer&) = delete;

    bool init(const KafkaConsumerConfig& config);
    bool subscribe(const std::vector<std::string>& topics);

    KafkaMessage poll(int timeout_ms);
    bool commit(const KafkaMessage& message);

    void close();
    static std::vector<KafkaLagRecord> queryLag(const KafkaConsumerConfig& config,
                                                const std::vector<std::string>& topics,
                                                int timeout_ms,
                                                std::string* error_message = nullptr);

private:
    void* consumer_;
};

}  // namespace nebula
