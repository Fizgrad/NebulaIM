#include "common/kafka/KafkaProducer.h"

#include "common/log/Logger.h"

#include <chrono>
#include <librdkafka/rdkafkacpp.h>
#include <utility>

namespace nebula {

class KafkaProducer::DeliveryReportCb : public RdKafka::DeliveryReportCb {
public:
    explicit DeliveryReportCb(KafkaProducer* owner) : owner_(owner) {}

    void dr_cb(RdKafka::Message& message) override {
        if (message.err()) {
            LOG_ERROR("Kafka delivery failed: " + message.errstr());
        }
        if (owner_ != nullptr) {
            owner_->recordDelivery(!message.err(), message.errstr());
        }
    }

private:
    KafkaProducer* owner_;
};

KafkaProducer::KafkaProducer()
    : delivery_cb_(std::make_unique<DeliveryReportCb>(this)),
      producer_(nullptr) {}

KafkaProducer::~KafkaProducer() {
    flush();
    delete static_cast<RdKafka::Producer*>(producer_);
    producer_ = nullptr;
}

bool KafkaProducer::init(const KafkaProducerConfig& config) {
    std::string errstr;
    std::unique_ptr<RdKafka::Conf> conf(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
    if (conf->set("bootstrap.servers", config.brokers, errstr) != RdKafka::Conf::CONF_OK ||
        conf->set("client.id", config.client_id, errstr) != RdKafka::Conf::CONF_OK ||
        conf->set("dr_cb", delivery_cb_.get(), errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERROR("Kafka producer config failed: " + errstr);
        return false;
    }

    RdKafka::Producer* producer = RdKafka::Producer::create(conf.get(), errstr);
    if (producer == nullptr) {
        LOG_ERROR("Kafka producer create failed: " + errstr);
        return false;
    }
    delete static_cast<RdKafka::Producer*>(producer_);
    producer_ = producer;
    return true;
}

bool KafkaProducer::produce(const std::string& topic, const std::string& key, const std::string& payload) {
    return produce(topic, key, payload, 5000);
}

bool KafkaProducer::produce(const std::string& topic, const std::string& key, const std::string& payload, int delivery_timeout_ms) {
    std::lock_guard<std::mutex> produce_lock(produce_mutex_);
    auto* producer = static_cast<RdKafka::Producer*>(producer_);
    if (producer == nullptr) {
        LOG_ERROR("Kafka producer is not initialized");
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(delivery_mutex_);
        deliveries_.clear();
    }
    RdKafka::ErrorCode err = producer->produce(topic,
                                               RdKafka::Topic::PARTITION_UA,
                                               RdKafka::Producer::RK_MSG_COPY,
                                               const_cast<char*>(payload.data()),
                                               payload.size(),
                                               key.data(),
                                               key.size(),
                                               0,
                                               nullptr,
                                               nullptr);
    producer->poll(0);
    if (err != RdKafka::ERR_NO_ERROR) {
        LOG_ERROR("Kafka produce failed: " + RdKafka::err2str(err));
        return false;
    }

    const int timeout_ms = delivery_timeout_ms > 0 ? delivery_timeout_ms : 5000;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        producer->poll(50);
        std::unique_lock<std::mutex> lock(delivery_mutex_);
        if (!deliveries_.empty()) {
            DeliveryStatus status = std::move(deliveries_.front());
            deliveries_.pop_front();
            if (!status.ok) {
                LOG_ERROR("Kafka delivery ack failed: " + status.error);
            }
            return status.ok;
        }
    }

    producer->poll(0);
    {
        std::unique_lock<std::mutex> lock(delivery_mutex_);
        if (!deliveries_.empty()) {
            DeliveryStatus status = std::move(deliveries_.front());
            deliveries_.pop_front();
            if (!status.ok) {
                LOG_ERROR("Kafka delivery ack failed: " + status.error);
            }
            return status.ok;
        }
    }
    LOG_ERROR("Kafka delivery ack timed out topic=" + topic + " key=" + key);
    return false;
}

void KafkaProducer::flush(int timeout_ms) {
    auto* producer = static_cast<RdKafka::Producer*>(producer_);
    if (producer != nullptr) {
        producer->flush(timeout_ms);
    }
}

void KafkaProducer::recordDelivery(bool ok, std::string error) {
    {
        std::lock_guard<std::mutex> lock(delivery_mutex_);
        deliveries_.push_back(DeliveryStatus{ok, std::move(error)});
    }
    delivery_cv_.notify_one();
}

}  // namespace nebula
