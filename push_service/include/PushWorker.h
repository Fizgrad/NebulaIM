#pragma once

#include "common.pb.h"
#include "common/kafka/KafkaConsumer.h"

#include <atomic>
#include <string>
#include <thread>

namespace nebula {

class PushDispatcher;

struct PushWorkerOptions {
    int poll_timeout_ms = 1000;
    std::string topic_single = "nebula.message.single";
    std::string topic_group = "nebula.message.group";
    std::string topic_retry = "nebula.message.retry";
};

class PushWorker {
public:
    PushWorker(KafkaConsumer* consumer, PushDispatcher* dispatcher, PushWorkerOptions options);
    ~PushWorker();

    bool start();
    void stop();

private:
    void run();
    void handleKafkaMessage(const KafkaMessage& message);
    void handleSingleMessage(const proto::MessageData& data);
    void handleGroupMessage(const proto::MessageData& data);
    void handleRetryMessage(const proto::MessageData& data);

private:
    KafkaConsumer* consumer_;
    PushDispatcher* dispatcher_;
    PushWorkerOptions options_;
    std::atomic<bool> running_;
    std::thread worker_thread_;
};

}  // namespace nebula
