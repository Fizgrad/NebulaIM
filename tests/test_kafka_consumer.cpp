#include "common/kafka/KafkaConsumer.h"
#include "common/kafka/KafkaProducer.h"
#include "common/utils/TimeUtil.h"
#include "tests/TestDeps.h"

#include <cassert>
#include <thread>

int main() {
    nebula::Config config;
    std::string reason;
    if (!nebula::tests::loadConfig(&config, &reason)) {
        return nebula::tests::skip("test_kafka_consumer", reason);
    }

    std::string unique = std::to_string(nebula::TimeUtil::nowMs());
    std::string key = "consumer-key-" + unique;
    std::string payload = "consumer-payload-" + unique;

    nebula::KafkaConsumerConfig consumer_config = nebula::tests::kafkaConsumerConfig(config);
    consumer_config.group_id = "nebula-test-consumer-" + unique;
    consumer_config.client_id = "nebula-test-client-" + unique;
    consumer_config.auto_offset_reset = "latest";
    consumer_config.enable_auto_commit = true;

    nebula::KafkaConsumer consumer;
    if (!consumer.init(consumer_config)) {
        return nebula::tests::skip("test_kafka_consumer", "Kafka consumer init failed");
    }
    if (!consumer.subscribe({"nebula.message.single"})) {
        return nebula::tests::skip("test_kafka_consumer", "Kafka topic subscription failed");
    }
    for (int i = 0; i < 10; ++i) {
        consumer.poll(100);
    }

    nebula::KafkaProducerConfig producer_config = nebula::tests::kafkaProducerConfig(config);
    producer_config.client_id = "nebula-test-producer-" + unique;
    nebula::KafkaProducer producer;
    if (!producer.init(producer_config)) {
        return nebula::tests::skip("test_kafka_consumer", "Kafka producer init failed");
    }
    if (!producer.produce("nebula.message.single", key, payload)) {
        return nebula::tests::skip("test_kafka_consumer", "Kafka broker is unavailable");
    }
    producer.flush(10000);

    bool found = false;
    for (int i = 0; i < 10; ++i) {
        nebula::KafkaMessage message = consumer.poll(3000);
        if (message.valid && message.key == key && message.payload == payload) {
            found = true;
            break;
        }
    }
    assert(found);
    consumer.close();
    return 0;
}
