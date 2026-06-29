#include "common/kafka/KafkaConsumer.h"

#include "common/log/Logger.h"

#include <librdkafka/rdkafkacpp.h>
#include <memory>

namespace nebula {

KafkaConsumer::KafkaConsumer()
    : consumer_(nullptr) {}

KafkaConsumer::~KafkaConsumer() {
    close();
}

bool KafkaConsumer::init(const KafkaConsumerConfig& config) {
    std::string errstr;
    std::unique_ptr<RdKafka::Conf> conf(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
    if (conf->set("bootstrap.servers", config.brokers, errstr) != RdKafka::Conf::CONF_OK ||
        conf->set("group.id", config.group_id, errstr) != RdKafka::Conf::CONF_OK ||
        conf->set("client.id", config.client_id, errstr) != RdKafka::Conf::CONF_OK ||
        conf->set("enable.auto.commit", config.enable_auto_commit ? "true" : "false", errstr) != RdKafka::Conf::CONF_OK ||
        conf->set("auto.offset.reset", "earliest", errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERROR("Kafka consumer config failed: " + errstr);
        return false;
    }
    RdKafka::KafkaConsumer* consumer = RdKafka::KafkaConsumer::create(conf.get(), errstr);
    if (consumer == nullptr) {
        LOG_ERROR("Kafka consumer create failed: " + errstr);
        return false;
    }
    close();
    consumer_ = consumer;
    return true;
}

bool KafkaConsumer::subscribe(const std::vector<std::string>& topics) {
    auto* consumer = static_cast<RdKafka::KafkaConsumer*>(consumer_);
    if (consumer == nullptr) return false;
    RdKafka::ErrorCode err = consumer->subscribe(topics);
    if (err != RdKafka::ERR_NO_ERROR) {
        LOG_ERROR("Kafka subscribe failed: " + RdKafka::err2str(err));
        return false;
    }
    return true;
}

KafkaMessage KafkaConsumer::poll(int timeout_ms) {
    KafkaMessage result;
    auto* consumer = static_cast<RdKafka::KafkaConsumer*>(consumer_);
    if (consumer == nullptr) return result;

    std::unique_ptr<RdKafka::Message> message(consumer->consume(timeout_ms));
    if (!message) return result;
    if (message->err() == RdKafka::ERR_NO_ERROR) {
        result.topic = message->topic_name();
        if (message->key()) result.key = *message->key();
        if (message->payload() != nullptr) result.payload.assign(static_cast<const char*>(message->payload()), message->len());
        result.partition = message->partition();
        result.offset = message->offset();
        result.valid = true;
    } else if (message->err() != RdKafka::ERR__TIMED_OUT) {
        LOG_ERROR("Kafka consume failed: " + message->errstr());
    }
    return result;
}

void KafkaConsumer::close() {
    auto* consumer = static_cast<RdKafka::KafkaConsumer*>(consumer_);
    if (consumer != nullptr) {
        consumer->close();
        delete consumer;
        consumer_ = nullptr;
    }
}

}  // namespace nebula
