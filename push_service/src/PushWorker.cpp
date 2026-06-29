#include "PushWorker.h"

#include "common/log/Logger.h"
#include "common/message/MessageKafkaPayload.h"
#include "common/push/PushDispatcher.h"

namespace nebula {

PushWorker::PushWorker(KafkaConsumer* consumer, PushDispatcher* dispatcher, PushWorkerOptions options)
    : consumer_(consumer), dispatcher_(dispatcher), options_(std::move(options)), running_(false) {}

PushWorker::~PushWorker() { stop(); }

bool PushWorker::start() {
    if (running_) return true;
    if (consumer_ == nullptr || dispatcher_ == nullptr) return false;
    running_ = true;
    worker_thread_ = std::thread([this]() { run(); });
    return true;
}

void PushWorker::stop() {
    if (!running_) return;
    running_ = false;
    if (worker_thread_.joinable()) worker_thread_.join();
}

void PushWorker::run() {
    while (running_) {
        KafkaMessage message = consumer_->poll(options_.poll_timeout_ms);
        if (!running_) break;
        if (!message.valid) continue;
        handleKafkaMessage(message);
    }
}

void PushWorker::handleKafkaMessage(const KafkaMessage& message) {
    proto::MessageData data;
    if (!MessageKafkaPayload::deserializeMessageData(message.payload, &data)) {
        LOG_ERROR("PushWorker failed to parse Kafka payload topic=" + message.topic);
        return;
    }
    if (message.topic == options_.topic_single) handleSingleMessage(data);
    else if (message.topic == options_.topic_group) handleGroupMessage(data);
    else if (message.topic == options_.topic_retry) handleRetryMessage(data);
}

void PushWorker::handleSingleMessage(const proto::MessageData& data) {
    dispatcher_->pushToUser("kafka-single", data.to_user_id(), data);
}

void PushWorker::handleGroupMessage(const proto::MessageData& data) {
    dispatcher_->pushToGroup("kafka-group", data.group_id(), data);
}

void PushWorker::handleRetryMessage(const proto::MessageData& data) {
    if (data.to_user_id() != 0) dispatcher_->pushToUser("kafka-retry", data.to_user_id(), data);
    else if (data.group_id() != 0) dispatcher_->pushToGroup("kafka-retry", data.group_id(), data);
}

}  // namespace nebula
