#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>

namespace nebula {

class KafkaProducer;
class OutboxDao;

struct OutboxWorkerOptions {
    int worker_interval_ms = 1000;
    size_t batch_size = 100;
    int max_retry_count = 5;
    int64_t retry_backoff_ms = 1000;
    int64_t claim_lease_ms = 30000;
    std::string dlq_topic = "nebula.message.dlq";
};

class OutboxWorker {
public:
    OutboxWorker(OutboxDao* outbox_dao, KafkaProducer* kafka_producer, OutboxWorkerOptions options = {});
    ~OutboxWorker();

    void start();
    void stop();
    bool running() const;

    void runOnce();

private:
    void loop();

private:
    OutboxDao* outbox_dao_;
    KafkaProducer* kafka_producer_;
    OutboxWorkerOptions options_;
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> worker_;
};

}  // namespace nebula
