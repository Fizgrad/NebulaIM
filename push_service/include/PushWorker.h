#pragma once

#include "common.pb.h"
#include "common/kafka/KafkaConsumer.h"

#include <atomic>
#include <string>
#include <thread>

namespace nebula {

class PushDispatcher;
class KafkaProducer;

struct PushWorkerOptions {
    int poll_timeout_ms = 1000;
    int failure_backoff_ms = 250;
    std::string topic_single = "nebula.message.single";
    std::string topic_group = "nebula.message.group";
    std::string topic_retry = "nebula.message.retry";
    std::string topic_dlq = "nebula.message.dlq";
};

class PushWorker {
public:
    PushWorker(KafkaConsumer* consumer, KafkaProducer* producer, PushDispatcher* dispatcher, PushWorkerOptions options);
    ~PushWorker();

    bool start();
    void stop();

private:
    void run();
    bool handleKafkaMessage(const KafkaMessage& message);
    bool handleSingleMessage(const proto::MessageData& data);
    bool handleGroupMessage(const proto::MessageData& data);
    bool handleRetryMessage(const proto::MessageData& data);

private:
    KafkaConsumer* consumer_;
    KafkaProducer* producer_;
    PushDispatcher* dispatcher_;
    PushWorkerOptions options_;
    std::atomic<bool> running_;
    std::thread worker_thread_;
};

}  // namespace nebula
