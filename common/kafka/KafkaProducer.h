#pragma once

#include <memory>
#include <string>

namespace nebula {

struct KafkaProducerConfig {
    std::string brokers = "127.0.0.1:9092";
    std::string client_id = "nebula-producer";
    int delivery_timeout_ms = 5000;
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

    std::unique_ptr<DeliveryReportCb> delivery_cb_;
    void* producer_;
    int delivery_timeout_ms_ = 5000;
};

}  // namespace nebula
