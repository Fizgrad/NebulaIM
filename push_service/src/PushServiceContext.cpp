#include "PushServiceContext.h"

#include "PushWorker.h"
#include "common/log/Logger.h"
#include "common/rpc/InternalRpcAuth.h"
#include "common/trace/TraceManager.h"

namespace nebula {

PushServiceContext::PushServiceContext() = default;
PushServiceContext::~PushServiceContext() { stopWorkers(); }

bool PushServiceContext::init(const std::string& config_path) {
    if (!config_.loadFromFile(config_path)) return false;
    InternalRpcAuth::instance().configureFromConfig(config_);
    TraceManager::instance().configure(TraceManager::configFrom(config_, "nebula-push-service"));
    MySqlConfig mysql;
    mysql.host = config_.getString("mysql.host", mysql.host);
    mysql.port = config_.getInt("mysql.port", mysql.port);
    mysql.user = config_.getString("mysql.user", mysql.user);
    mysql.password = config_.getString("mysql.password", mysql.password);
    mysql.database = config_.getString("mysql.database", mysql.database);
    if (!mysql_pool_.init(mysql, static_cast<size_t>(config_.getInt("mysql.pool_size", 4)))) return false;

    redis_client_ = std::make_unique<RedisClient>();
    RedisConfig redis;
    redis.host = config_.getString("redis.host", redis.host);
    redis.port = config_.getInt("redis.port", redis.port);
    redis.timeout_ms = config_.getInt("redis.timeout_ms", redis.timeout_ms);
    redis.password = config_.getString("redis.password", redis.password);
    if (!redis_client_->connect(redis)) return false;

    kafka_producer_ = std::make_unique<KafkaProducer>();
    KafkaProducerConfig producer_config;
    producer_config.brokers = config_.getString("kafka.brokers", producer_config.brokers);
    producer_config.client_id = config_.getString("kafka.producer.client_id", "nebula-push-producer");
    if (!kafka_producer_->init(producer_config)) return false;

    kafka_consumer_config_.brokers = config_.getString("kafka.brokers", kafka_consumer_config_.brokers);
    kafka_consumer_config_.group_id = config_.getString("kafka.consumer.group_id", "nebula-push-service");
    kafka_consumer_config_.client_id = config_.getString("kafka.consumer.client_id", "nebula-push-consumer");
    kafka_consumer_config_.enable_auto_commit = config_.getBool("kafka.consumer.enable_auto_commit", kafka_consumer_config_.enable_auto_commit);

    options_.worker_num = config_.getInt("push_service.worker_num", 1);
    options_.poll_timeout_ms = config_.getInt("push_service.poll_timeout_ms", 1000);
    options_.max_retry_count = config_.getInt("push_service.max_retry_count", 3);
    options_.retry_ttl_seconds = config_.getInt("push_service.retry_ttl_seconds", 86400);
    options_.skip_sender_for_group = config_.getBool("push_service.skip_sender_for_group", true);
    options_.topic_single = config_.getString("kafka.topic.single", options_.topic_single);
    options_.topic_group = config_.getString("kafka.topic.group", options_.topic_group);
    options_.topic_offline = config_.getString("kafka.topic.offline", options_.topic_offline);
    options_.topic_retry = config_.getString("kafka.topic.retry", options_.topic_retry);
    options_.topic_dlq = config_.getString("kafka.topic.dlq", options_.topic_dlq);
    grpc_tls_config_ = GrpcTlsCredentials::fromConfig(config_);

    offline_message_dao_ = std::make_unique<OfflineMessageDao>(mysql_pool_);
    group_dao_ = std::make_unique<GroupDao>(mysql_pool_);
    online_status_manager_ = std::make_unique<OnlineStatusManager>(redis_client_.get());
    gateway_client_manager_ = std::make_unique<GatewayClientManager>();
    service_resolver_ = std::make_unique<StaticServiceResolver>();
    service_resolver_->loadFromConfig(config_);
    if (!gateway_client_manager_->initFromResolver(*service_resolver_, grpc_tls_config_)) return false;
    retry_manager_ = std::make_unique<PushRetryManager>(redis_client_.get(), options_.max_retry_count, options_.retry_ttl_seconds);
    PushOptions push_options;
    push_options.topic_retry = options_.topic_retry;
    push_options.topic_dlq = options_.topic_dlq;
    push_options.skip_sender_for_group = options_.skip_sender_for_group;
    dispatcher_ = std::make_unique<PushDispatcher>(online_status_manager_.get(), gateway_client_manager_.get(), offline_message_dao_.get(), group_dao_.get(), kafka_producer_.get(), retry_manager_.get(), push_options);

    listen_address_ = config_.getString("push_service.host", "0.0.0.0") + ":" + std::to_string(config_.getInt("push_service.port", 50054));
    return true;
}

bool PushServiceContext::startWorkers() {
    if (!workers_.empty()) return true;
    int worker_count = options_.worker_num > 0 ? options_.worker_num : 1;
    for (int i = 0; i < worker_count; ++i) {
        auto consumer = std::make_unique<KafkaConsumer>();
        KafkaConsumerConfig config = kafka_consumer_config_;
        config.client_id = kafka_consumer_config_.client_id + "-" + std::to_string(i);
        if (!consumer->init(config)) {
            stopWorkers();
            return false;
        }
        if (!consumer->subscribe({options_.topic_single, options_.topic_group, options_.topic_retry})) {
            stopWorkers();
            return false;
        }
        PushWorkerOptions worker_options;
        worker_options.poll_timeout_ms = options_.poll_timeout_ms;
        worker_options.topic_single = options_.topic_single;
        worker_options.topic_group = options_.topic_group;
        worker_options.topic_retry = options_.topic_retry;
        auto worker = std::make_unique<PushWorker>(consumer.get(), dispatcher_.get(), worker_options);
        if (!worker->start()) {
            stopWorkers();
            return false;
        }
        kafka_consumers_.push_back(std::move(consumer));
        workers_.push_back(std::move(worker));
    }
    return true;
}

void PushServiceContext::stopWorkers() {
    for (auto& worker : workers_) worker->stop();
    workers_.clear();
    for (auto& consumer : kafka_consumers_) {
        if (consumer) consumer->close();
    }
    kafka_consumers_.clear();
}

PushDispatcher* PushServiceContext::dispatcher() { return dispatcher_.get(); }
PushServiceOptions PushServiceContext::options() const { return options_; }
std::string PushServiceContext::listenAddress() const { return listen_address_; }
const GrpcTlsConfig& PushServiceContext::grpcTlsConfig() const { return grpc_tls_config_; }

}  // namespace nebula
