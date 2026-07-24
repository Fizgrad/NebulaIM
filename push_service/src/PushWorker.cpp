#include "PushWorker.h"

#include "common/kafka/KafkaProducer.h"
#include "common/log/Logger.h"
#include "common/message/MessageKafkaPayload.h"
#include "common/monitor/MetricsRegistry.h"
#include "common/push/PushDispatcher.h"
#include "common/trace/TraceContext.h"
#include "common/trace/TraceSpan.h"

#include <chrono>

namespace nebula {

PushWorker::PushWorker(KafkaConsumer* consumer, KafkaProducer* producer, PushDispatcher* dispatcher, PushWorkerOptions options)
    : consumer_(consumer), producer_(producer), dispatcher_(dispatcher), options_(std::move(options)), running_(false) {}

PushWorker::~PushWorker() { stop(); }

bool PushWorker::start() {
    if (consumer_ == nullptr || producer_ == nullptr || dispatcher_ == nullptr) return false;
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return true;
    worker_thread_ = std::thread([this]() { run(); });
    return true;
}

void PushWorker::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) return;
    if (worker_thread_.joinable()) worker_thread_.join();
}

void PushWorker::run() {
    while (running_) {
        KafkaMessage message = consumer_->poll(options_.poll_timeout_ms);
        if (!running_) break;
        if (!message.valid) continue;
        MetricsRegistry::instance().counter("nebula_push_kafka_consume_total", "PushService Kafka messages consumed").inc();
        LOG_INFO("PushWorker polled topic=" + message.topic +
                 " partition=" + std::to_string(message.partition) +
                 " offset=" + std::to_string(message.offset) +
                 " payload_bytes=" + std::to_string(message.payload.size()));
        if (handleKafkaMessage(message)) {
            MetricsRegistry::instance().counter("nebula_push_kafka_handle_success_total", "PushService Kafka messages handled successfully").inc();
            bool committed = consumer_->commit(message);
            if (committed) {
                MetricsRegistry::instance().counter("nebula_push_kafka_commit_total", "PushService Kafka offsets committed").inc();
            } else {
                MetricsRegistry::instance().counter("nebula_push_kafka_commit_failed_total", "PushService Kafka offset commit failures").inc();
                if (!consumer_->seek(message)) {
                    LOG_ERROR("PushWorker failed to rewind after commit failure; stopping to preserve ordering");
                    running_ = false;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(options_.failure_backoff_ms));
            }
            LOG_INFO("PushWorker handled topic=" + message.topic +
                     " partition=" + std::to_string(message.partition) +
                     " offset=" + std::to_string(message.offset) +
                     " committed=" + (committed ? "true" : "false"));
        } else {
            MetricsRegistry::instance().counter("nebula_push_kafka_handle_failed_total", "PushService Kafka message handle failures").inc();
            LOG_ERROR("PushWorker message handling failed topic=" + message.topic +
                      " partition=" + std::to_string(message.partition) +
                      " offset=" + std::to_string(message.offset));
            if (!consumer_->seek(message)) {
                LOG_ERROR("PushWorker failed to rewind Kafka offset; stopping to prevent message loss");
                running_ = false;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(options_.failure_backoff_ms));
        }
    }
}

bool PushWorker::handleKafkaMessage(const KafkaMessage& message) {
    proto::MessageData data;
    if (!MessageKafkaPayload::deserializeMessageData(message.payload, &data)) {
        LOG_ERROR("PushWorker failed to parse Kafka payload topic=" + message.topic +
                  " partition=" + std::to_string(message.partition) +
                  " offset=" + std::to_string(message.offset) +
                  " payload_bytes=" + std::to_string(message.payload.size()));
        return producer_ != nullptr && producer_->produce(options_.topic_dlq, message.key, message.payload);
    }
    TraceContext::Scope trace(TraceContext::ensureTraceId(data.trace_id()));
    TraceSpan span("push.kafka.consume", TraceSpanKind::CONSUMER);
    span.setAttribute("topic", message.topic);
    span.setAttribute("partition", std::to_string(message.partition));
    span.setAttribute("offset", std::to_string(message.offset));
    span.setAttribute("message_id", std::to_string(data.message_id()));
    if (message.topic == options_.topic_single) return handleSingleMessage(data);
    if (message.topic == options_.topic_group) return handleGroupMessage(data);
    if (message.topic == options_.topic_retry) return handleRetryMessage(data);
    return producer_ != nullptr && producer_->produce(options_.topic_dlq, message.key, message.payload);
}

bool PushWorker::handleSingleMessage(const proto::MessageData& data) {
    if (data.message_id() == 0 || data.from_user_id() == 0 || data.to_user_id() == 0) {
        return producer_->produce(options_.topic_dlq, std::to_string(data.message_id()),
                                  MessageKafkaPayload::serializeMessageData(data));
    }
    return dispatcher_->pushToUser("kafka-single", data.to_user_id(), data);
}

bool PushWorker::handleGroupMessage(const proto::MessageData& data) {
    if (data.message_id() == 0 || data.from_user_id() == 0 || data.group_id() == 0) {
        return producer_->produce(options_.topic_dlq, std::to_string(data.message_id()),
                                  MessageKafkaPayload::serializeMessageData(data));
    }
    PushResult result = dispatcher_->pushToGroup("kafka-group", data.group_id(), data);
    return result.failed_count == 0;
}

bool PushWorker::handleRetryMessage(const proto::MessageData& data) {
    if (data.to_user_id() != 0) return dispatcher_->pushToUser("kafka-retry", data.to_user_id(), data);
    if (data.group_id() != 0) {
        PushResult result = dispatcher_->pushToGroup("kafka-retry", data.group_id(), data);
        return result.failed_count == 0;
    }
    return producer_->produce(options_.topic_dlq, std::to_string(data.message_id()),
                              MessageKafkaPayload::serializeMessageData(data));
}

}  // namespace nebula
