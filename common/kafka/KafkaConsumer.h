#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nebula {

struct KafkaConsumerConfig {
    std::string brokers = "127.0.0.1:9092";
    std::string group_id = "nebula-consumer";
    std::string client_id = "nebula-consumer-client";
    bool enable_auto_commit = true;
};

struct KafkaMessage {
    std::string topic;
    std::string key;
    std::string payload;
    int partition = 0;
    int64_t offset = 0;
    bool valid = false;
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

    void close();

private:
    void* consumer_;
};

}  // namespace nebula
