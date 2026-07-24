#include "common/kafka/KafkaProducer.h"

#include "common/log/Logger.h"

#include <chrono>
#include <librdkafka/rdkafkacpp.h>
#include <mutex>
#include <utility>

namespace nebula {

namespace {

struct DeliveryState {
    std::mutex mutex;
    bool done = false;
    bool ok = false;
    std::string error;
};

using DeliveryHandle = std::shared_ptr<DeliveryState>;

}  // namespace

class KafkaProducer::DeliveryReportCb : public RdKafka::DeliveryReportCb {
public:
    void dr_cb(RdKafka::Message& message) override {
        auto* handle = static_cast<DeliveryHandle*>(message.msg_opaque());
        if (handle == nullptr) return;
        DeliveryHandle state = std::move(*handle);
        delete handle;
        if (message.err()) {
            LOG_ERROR("Kafka delivery failed: " + message.errstr());
        }
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->done = true;
            state->ok = !message.err();
            state->error = message.errstr();
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
        conf->set("message.timeout.ms", std::to_string(config.delivery_timeout_ms > 0 ? config.delivery_timeout_ms : 5000), errstr) !=
            RdKafka::Conf::CONF_OK ||
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
    delivery_timeout_ms_ = config.delivery_timeout_ms > 0 ? config.delivery_timeout_ms : 5000;
    return true;
}

bool KafkaProducer::produce(const std::string& topic, const std::string& key, const std::string& payload) {
    return produce(topic, key, payload, delivery_timeout_ms_ + 250);
}

bool KafkaProducer::produce(const std::string& topic, const std::string& key, const std::string& payload, int delivery_timeout_ms) {
    auto* producer = static_cast<RdKafka::Producer*>(producer_);
    if (producer == nullptr) {
        LOG_ERROR("Kafka producer is not initialized");
        return false;
    }
    DeliveryHandle state = std::make_shared<DeliveryState>();
    auto* delivery_handle = new DeliveryHandle(state);
    RdKafka::ErrorCode err = producer->produce(topic,
                                               RdKafka::Topic::PARTITION_UA,
                                               RdKafka::Producer::RK_MSG_COPY,
                                               const_cast<char*>(payload.data()),
                                               payload.size(),
                                               key.data(),
                                               key.size(),
                                               0,
                                               nullptr,
                                               delivery_handle);
    producer->poll(0);
    if (err != RdKafka::ERR_NO_ERROR) {
        delete delivery_handle;
        LOG_ERROR("Kafka produce failed: " + RdKafka::err2str(err));
        return false;
    }

    const int timeout_ms = delivery_timeout_ms > 0 ? delivery_timeout_ms : 5000;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        producer->poll(50);
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->done) {
            if (!state->ok) {
                LOG_ERROR("Kafka delivery ack failed: " + state->error);
            }
            return state->ok;
        }
    }

    producer->poll(0);
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->done) {
            if (!state->ok) {
                LOG_ERROR("Kafka delivery ack failed: " + state->error);
            }
            return state->ok;
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

}  // namespace nebula
