#pragma once

#include "common/outbox/OutboxWorker.h"

#include <memory>

namespace nebula {

class KafkaProducer;
class OutboxDao;

class MessageOutboxWorkerContext {
public:
    MessageOutboxWorkerContext(OutboxDao* outbox_dao, KafkaProducer* kafka_producer, OutboxWorkerOptions options);
    ~MessageOutboxWorkerContext();

    void start();
    void stop();
    bool running() const;

private:
    std::unique_ptr<OutboxWorker> worker_;
};

}  // namespace nebula
