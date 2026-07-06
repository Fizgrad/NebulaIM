#pragma once

#include "common/config/Config.h"
#include "common/discovery/StaticServiceResolver.h"
#include "common/dao/GroupDao.h"
#include "common/dao/OfflineMessageDao.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/kafka/KafkaConsumer.h"
#include "common/kafka/KafkaProducer.h"
#include "common/push/GatewayClientManager.h"
#include "common/push/OnlineStatusManager.h"
#include "common/push/PushDispatcher.h"
#include "common/push/PushRetryManager.h"
#include "common/redis/RedisClient.h"
#include "common/rpc/GrpcTlsCredentials.h"

#include <memory>
#include <string>
#include <vector>

namespace nebula {

class PushWorker;

struct PushServiceOptions {
    int worker_num = 1;
    int poll_timeout_ms = 1000;
    int max_retry_count = 3;
    int retry_ttl_seconds = 86400;
    bool skip_sender_for_group = true;
    std::string topic_single = "nebula.message.single";
    std::string topic_group = "nebula.message.group";
    std::string topic_offline = "nebula.message.offline";
    std::string topic_retry = "nebula.message.retry";
    std::string topic_dlq = "nebula.message.dlq";
};

class PushServiceContext {
public:
    PushServiceContext();
    ~PushServiceContext();

    bool init(const std::string& config_path);
    bool startWorkers();
    void stopWorkers();

    PushDispatcher* dispatcher();
    PushServiceOptions options() const;
    std::string listenAddress() const;
    const GrpcTlsConfig& grpcTlsConfig() const;

private:
    Config config_;
    MySqlConnectionPool mysql_pool_;
    std::unique_ptr<RedisClient> redis_client_;
    std::unique_ptr<KafkaProducer> kafka_producer_;
    KafkaConsumerConfig kafka_consumer_config_;
    std::vector<std::unique_ptr<KafkaConsumer>> kafka_consumers_;
    std::unique_ptr<OfflineMessageDao> offline_message_dao_;
    std::unique_ptr<GroupDao> group_dao_;
    std::unique_ptr<OnlineStatusManager> online_status_manager_;
    std::unique_ptr<GatewayClientManager> gateway_client_manager_;
    std::unique_ptr<StaticServiceResolver> service_resolver_;
    std::unique_ptr<PushRetryManager> retry_manager_;
    std::unique_ptr<PushDispatcher> dispatcher_;
    std::vector<std::unique_ptr<PushWorker>> workers_;
    PushServiceOptions options_;
    std::string listen_address_;
    GrpcTlsConfig grpc_tls_config_;
};

}  // namespace nebula
