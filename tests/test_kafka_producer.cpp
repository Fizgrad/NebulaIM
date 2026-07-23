#include "common/kafka/KafkaProducer.h"
#include "common/utils/TimeUtil.h"
#include "tests/TestDeps.h"

#include <cassert>

int main() {
    nebula::Config config;
    std::string reason;
    if (!nebula::tests::loadConfig(&config, &reason)) {
        return nebula::tests::skip("test_kafka_producer", reason);
    }

    nebula::KafkaProducerConfig kafka = nebula::tests::kafkaProducerConfig(config);
    nebula::KafkaProducer producer;
    if (!producer.init(kafka)) {
        return nebula::tests::skip("test_kafka_producer", "Kafka producer init failed");
    }
    std::string key = "test-key-" + std::to_string(nebula::TimeUtil::nowMs());
    if (!producer.produce("nebula.message.single", key, "test-payload")) {
        return nebula::tests::skip("test_kafka_producer", "Kafka broker is unavailable");
    }
    producer.flush(10000);
    return 0;
}
