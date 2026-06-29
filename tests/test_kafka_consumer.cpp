#include "common/config/Config.h"
#include "common/kafka/KafkaConsumer.h"
#include "common/kafka/KafkaProducer.h"
#include "common/utils/TimeUtil.h"

#include <cassert>
#include <thread>

int main() {
    nebula::Config config;
    assert(config.loadFromFile("config/nebula.conf"));

    std::string brokers = config.getString("kafka.brokers", "127.0.0.1:9092");
    std::string unique = std::to_string(nebula::TimeUtil::nowMs());
    std::string key = "consumer-key-" + unique;
    std::string payload = "consumer-payload-" + unique;

    nebula::KafkaConsumerConfig consumer_config;
    consumer_config.brokers = brokers;
    consumer_config.group_id = "nebula-test-consumer-" + unique;
    consumer_config.client_id = "nebula-test-client-" + unique;
    consumer_config.enable_auto_commit = true;

    nebula::KafkaConsumer consumer;
    assert(consumer.init(consumer_config));
    assert(consumer.subscribe({"nebula.message.single"}));

    nebula::KafkaProducerConfig producer_config;
    producer_config.brokers = brokers;
    producer_config.client_id = "nebula-test-producer-" + unique;
    nebula::KafkaProducer producer;
    assert(producer.init(producer_config));
    assert(producer.produce("nebula.message.single", key, payload));
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
