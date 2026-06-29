#include "common/config/Config.h"
#include "common/kafka/KafkaProducer.h"
#include "common/message/MessageKafkaPayload.h"
#include "common/utils/TimeUtil.h"

#include <iostream>

int main() {
    nebula::Config config;
    if (!config.loadFromFile("config/nebula.conf")) return 1;
    nebula::KafkaProducerConfig kafka;
    kafka.brokers = config.getString("kafka.brokers", kafka.brokers);
    kafka.client_id = "push-kafka-demo";
    nebula::KafkaProducer producer;
    if (!producer.init(kafka)) return 1;

    nebula::proto::MessageData data;
    data.set_message_id(static_cast<uint64_t>(nebula::TimeUtil::nowMs()));
    data.set_conversation_id(1);
    data.set_from_user_id(10000);
    data.set_to_user_id(10001);
    data.set_content_type(nebula::proto::MESSAGE_CONTENT_TYPE_TEXT);
    data.set_content("push kafka demo");
    data.set_status(nebula::proto::MESSAGE_STATUS_SENT);
    data.set_timestamp(nebula::TimeUtil::nowMs());
    std::string payload = nebula::MessageKafkaPayload::serializeMessageData(data);
    producer.produce(config.getString("kafka.topic.single", "nebula.message.single"), std::to_string(data.conversation_id()), payload);
    data.set_group_id(1);
    payload = nebula::MessageKafkaPayload::serializeMessageData(data);
    producer.produce(config.getString("kafka.topic.group", "nebula.message.group"), std::to_string(data.group_id()), payload);
    producer.flush(5000);
    std::cout << "push kafka demo ok" << std::endl;
    return 0;
}
