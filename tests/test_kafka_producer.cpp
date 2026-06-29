#include "common/config/Config.h"
#include "common/kafka/KafkaProducer.h"
#include "common/utils/TimeUtil.h"

#include <cassert>

int main() {
    nebula::Config config;
    assert(config.loadFromFile("config/nebula.conf"));

    nebula::KafkaProducerConfig kafka;
    kafka.brokers = config.getString("kafka.brokers", kafka.brokers);
    kafka.client_id = config.getString("kafka.producer.client_id", kafka.client_id);

    nebula::KafkaProducer producer;
    assert(producer.init(kafka));
    std::string key = "test-key-" + std::to_string(nebula::TimeUtil::nowMs());
    assert(producer.produce("nebula.message.single", key, "test-payload"));
    producer.flush(10000);
    return 0;
}
