#include "common/kafka/KafkaProducer.h"

#include "common/log/Logger.h"

#include <librdkafka/rdkafkacpp.h>

namespace nebula {

class KafkaProducer::DeliveryReportCb : public RdKafka::DeliveryReportCb {
public:
    void dr_cb(RdKafka::Message& message) override {
        if (message.err()) {
            LOG_ERROR("Kafka delivery failed: " + message.errstr());
        }
    }
};

KafkaProducer::KafkaProducer()
    : delivery_cb_(std::make_unique<DeliveryReportCb>()),
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
    auto* producer = static_cast<RdKafka::Producer*>(producer_);
    if (producer == nullptr) {
        LOG_ERROR("Kafka producer is not initialized");
        return false;
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
    return true;
}

void KafkaProducer::flush(int timeout_ms) {
    auto* producer = static_cast<RdKafka::Producer*>(producer_);
    if (producer != nullptr) {
        producer->flush(timeout_ms);
    }
}

}  // namespace nebula
