#include "MessageOutboxWorkerContext.h"

#include <utility>

namespace nebula {

MessageOutboxWorkerContext::MessageOutboxWorkerContext(OutboxDao* outbox_dao, KafkaProducer* kafka_producer, OutboxWorkerOptions options)
    : worker_(std::make_unique<OutboxWorker>(outbox_dao, kafka_producer, std::move(options))) {}

MessageOutboxWorkerContext::~MessageOutboxWorkerContext() {
    stop();
}

void MessageOutboxWorkerContext::start() {
    worker_->start();
}

void MessageOutboxWorkerContext::stop() {
    worker_->stop();
}

bool MessageOutboxWorkerContext::running() const {
    return worker_->running();
}

}  // namespace nebula
