#include "common/outbox/OutboxWorker.h"

#include "common/kafka/KafkaProducer.h"
#include "common/log/Logger.h"
#include "common/outbox/OutboxDao.h"
#include "common/utils/TimeUtil.h"

#include <chrono>

namespace nebula {

OutboxWorker::OutboxWorker(OutboxDao* outbox_dao, KafkaProducer* kafka_producer, OutboxWorkerOptions options)
    : outbox_dao_(outbox_dao),
      kafka_producer_(kafka_producer),
      options_(std::move(options)),
      running_(false) {}

OutboxWorker::~OutboxWorker() {
    stop();
}

void OutboxWorker::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return;
    worker_ = std::make_unique<std::thread>([this]() { loop(); });
}

void OutboxWorker::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) return;
    if (worker_ && worker_->joinable()) worker_->join();
    worker_.reset();
}

bool OutboxWorker::running() const {
    return running_.load(std::memory_order_relaxed);
}

void OutboxWorker::runOnce() {
    if (outbox_dao_ == nullptr || kafka_producer_ == nullptr) return;
    for (size_t processed = 0; processed < options_.batch_size; ++processed) {
        auto events = outbox_dao_->claimPendingEvents(1, TimeUtil::nowMs(), options_.claim_lease_ms);
        if (events.empty()) break;
        const auto& event = events.front();
        bool ok = kafka_producer_->produce(event.topic, event.event_key, event.payload);
        if (ok) {
            if (!outbox_dao_->markPublished(event.event_id, event.claim_token)) {
                LOG_ERROR("outbox publish ownership lost event_id=" + std::to_string(event.event_id));
            }
            continue;
        }

        int retry_count = event.retry_count + 1;
        if (retry_count > options_.max_retry_count) {
            bool dlq_ok = !options_.dlq_topic.empty() &&
                          kafka_producer_->produce(options_.dlq_topic, event.event_key, event.payload);
            if (dlq_ok) {
                if (!outbox_dao_->markDead(event.event_id, event.claim_token)) {
                    LOG_ERROR("outbox dead-state ownership lost event_id=" + std::to_string(event.event_id));
                }
                LOG_ERROR("outbox event moved to dead status event_id=" + std::to_string(event.event_id));
            } else {
                int64_t backoff = options_.retry_backoff_ms * retry_count;
                outbox_dao_->markFailed(event.event_id, event.claim_token, event.retry_count,
                                        TimeUtil::nowMs() + backoff);
                LOG_ERROR("outbox DLQ publish failed; event retained for retry event_id=" +
                          std::to_string(event.event_id));
            }
            continue;
        }

        int64_t backoff = options_.retry_backoff_ms * retry_count;
        outbox_dao_->markFailed(event.event_id, event.claim_token, retry_count, TimeUtil::nowMs() + backoff);
    }
}

void OutboxWorker::loop() {
    while (running_.load(std::memory_order_relaxed)) {
        runOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(options_.worker_interval_ms));
    }
}

}  // namespace nebula
