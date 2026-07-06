#pragma once

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

namespace nebula {

struct KafkaProducerConfig {
    std::string brokers = "127.0.0.1:9092";
    std::string client_id = "nebula-producer";
};

class KafkaProducer {
public:
    KafkaProducer();
    ~KafkaProducer();

    KafkaProducer(const KafkaProducer&) = delete;
    KafkaProducer& operator=(const KafkaProducer&) = delete;

    bool init(const KafkaProducerConfig& config);

    bool produce(const std::string& topic,
                 const std::string& key,
                 const std::string& payload);
    bool produce(const std::string& topic,
                 const std::string& key,
                 const std::string& payload,
                 int delivery_timeout_ms);

    void flush(int timeout_ms = 5000);

private:
    class DeliveryReportCb;
    struct DeliveryStatus {
        bool ok = false;
        std::string error;
    };

    void recordDelivery(bool ok, std::string error);

    std::unique_ptr<DeliveryReportCb> delivery_cb_;
    void* producer_;
    std::mutex produce_mutex_;
    std::mutex delivery_mutex_;
    std::condition_variable delivery_cv_;
    std::deque<DeliveryStatus> deliveries_;
};

}  // namespace nebula
