#include "common/kafka/KafkaConsumer.h"

#include "common/log/Logger.h"

#include <librdkafka/rdkafkacpp.h>
#include <memory>
#include <unordered_set>

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
        conf->set("enable.auto.offset.store", config.enable_auto_commit ? "true" : "false", errstr) != RdKafka::Conf::CONF_OK ||
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

bool KafkaConsumer::commit(const KafkaMessage& message) {
    auto* consumer = static_cast<RdKafka::KafkaConsumer*>(consumer_);
    if (consumer == nullptr || !message.valid) return false;
    std::vector<RdKafka::TopicPartition*> offsets;
    offsets.push_back(RdKafka::TopicPartition::create(message.topic, message.partition, message.offset + 1));
    RdKafka::ErrorCode err = consumer->commitSync(offsets);
    RdKafka::TopicPartition::destroy(offsets);
    if (err != RdKafka::ERR_NO_ERROR) {
        LOG_ERROR("Kafka commit failed: " + RdKafka::err2str(err));
        return false;
    }
    return true;
}

void KafkaConsumer::close() {
    auto* consumer = static_cast<RdKafka::KafkaConsumer*>(consumer_);
    if (consumer != nullptr) {
        consumer->close();
        delete consumer;
        consumer_ = nullptr;
    }
}

std::vector<KafkaLagRecord> KafkaConsumer::queryLag(const KafkaConsumerConfig& config,
                                                    const std::vector<std::string>& topics,
                                                    int timeout_ms,
                                                    std::string* error_message) {
    std::vector<KafkaLagRecord> records;
    std::string errstr;
    std::unique_ptr<RdKafka::Conf> conf(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
    if (conf->set("bootstrap.servers", config.brokers, errstr) != RdKafka::Conf::CONF_OK ||
        conf->set("group.id", config.group_id, errstr) != RdKafka::Conf::CONF_OK ||
        conf->set("client.id", config.client_id.empty() ? "nebula-admin-lag" : config.client_id + "-lag", errstr) != RdKafka::Conf::CONF_OK ||
        conf->set("enable.auto.commit", "false", errstr) != RdKafka::Conf::CONF_OK) {
        if (error_message != nullptr) *error_message = errstr;
        return records;
    }

    std::unique_ptr<RdKafka::KafkaConsumer> consumer(RdKafka::KafkaConsumer::create(conf.get(), errstr));
    if (!consumer) {
        if (error_message != nullptr) *error_message = errstr;
        return records;
    }

    std::unique_ptr<RdKafka::Metadata> metadata;
    RdKafka::Metadata* raw_metadata = nullptr;
    RdKafka::ErrorCode meta_err = consumer->metadata(true, nullptr, &raw_metadata, timeout_ms);
    metadata.reset(raw_metadata);
    if (meta_err != RdKafka::ERR_NO_ERROR || !metadata) {
        if (error_message != nullptr) *error_message = RdKafka::err2str(meta_err);
        consumer->close();
        return records;
    }

    std::unordered_set<std::string> wanted(topics.begin(), topics.end());
    std::vector<RdKafka::TopicPartition*> partitions;
    for (const auto* topic_meta : *metadata->topics()) {
        if (topic_meta == nullptr || topic_meta->err() != RdKafka::ERR_NO_ERROR) continue;
        if (!wanted.empty() && wanted.find(topic_meta->topic()) == wanted.end()) continue;
        for (const auto* partition_meta : *topic_meta->partitions()) {
            if (partition_meta == nullptr || partition_meta->err() != RdKafka::ERR_NO_ERROR) continue;
            partitions.push_back(RdKafka::TopicPartition::create(topic_meta->topic(), partition_meta->id()));
        }
    }

    if (!partitions.empty()) {
        RdKafka::ErrorCode committed_err = consumer->committed(partitions, timeout_ms);
        if (committed_err != RdKafka::ERR_NO_ERROR && error_message != nullptr) {
            *error_message = RdKafka::err2str(committed_err);
        }
        for (const auto* partition : partitions) {
            if (partition == nullptr) continue;
            int64_t low = 0;
            int64_t high = 0;
            RdKafka::ErrorCode water_err = consumer->query_watermark_offsets(partition->topic(), partition->partition(), &low, &high, timeout_ms);
            if (water_err != RdKafka::ERR_NO_ERROR) continue;
            int64_t committed = partition->offset();
            if (committed < 0) committed = low;
            KafkaLagRecord record;
            record.topic = partition->topic();
            record.consumer_group = config.group_id;
            record.partition = partition->partition();
            record.committed_offset = committed;
            record.high_offset = high;
            record.lag = high > committed ? high - committed : 0;
            records.push_back(std::move(record));
        }
    }
    RdKafka::TopicPartition::destroy(partitions);
    consumer->close();
    return records;
}

}  // namespace nebula
