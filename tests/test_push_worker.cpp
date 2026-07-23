#include "TestDeps.h"
#include "PushServiceContext.h"
#include "common/kafka/KafkaProducer.h"
#include "common/message/MessageKafkaPayload.h"
#include "common/utils/TimeUtil.h"

#include <cassert>
#include <thread>

int main() {
    nebula::PushServiceContext context;
    if (!context.init("config/nebula.conf")) return nebula::tests::skip("test_push_worker", "PushService dependencies are not reachable");
    assert(context.startWorkers());

    nebula::Config config;
    assert(config.loadFromFile("config/nebula.conf"));
    nebula::KafkaProducerConfig producer_config;
    producer_config.brokers = config.getString("kafka.brokers", producer_config.brokers);
    producer_config.client_id = "push-worker-test";
    nebula::KafkaProducer producer;
    assert(producer.init(producer_config));

    nebula::proto::MessageData data;
    data.set_message_id(static_cast<uint64_t>(nebula::TimeUtil::nowMs()));
    data.set_conversation_id(1);
    data.set_from_user_id(1);
    data.set_to_user_id(94001);
    data.set_content_type(nebula::proto::MESSAGE_CONTENT_TYPE_TEXT);
    data.set_content("worker offline");
    data.set_status(nebula::proto::MESSAGE_STATUS_SENT);
    data.set_timestamp(nebula::TimeUtil::nowMs());
    assert(producer.produce(config.getString("kafka.topic.single", "nebula.message.single"), "1", nebula::MessageKafkaPayload::serializeMessageData(data)));
    producer.flush(5000);
    std::this_thread::sleep_for(std::chrono::seconds(3));
    context.stopWorkers();
    return 0;
}
