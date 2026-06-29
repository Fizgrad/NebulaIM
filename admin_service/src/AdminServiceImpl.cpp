#include "AdminServiceImpl.h"

#include "common/error/ErrorCode.h"
#include "common/health/HealthStatus.h"
#include "common/log/Logger.h"
#include "common/rpc/RpcMetadata.h"
#include "common/utils/TimeUtil.h"

#if defined(NEBULA_ENABLE_STORAGE)
#include "common/db/MySqlConnection.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/db/MySqlResult.h"
#include "common/kafka/KafkaConsumer.h"
#include "common/redis/RedisClient.h"
#endif

#include <unordered_map>
#include <utility>

namespace nebula {

namespace {
void fillResponse(proto::CommonResponse* response, const std::string& request_id, ErrorCode code, const std::string& message = "") {
    response->set_code(static_cast<int32_t>(code));
    response->set_message(message.empty() ? errorCodeToString(code) : message);
    response->set_request_id(request_id.empty() ? "request-id-empty" : request_id);
}
}

AdminServiceImpl::AdminServiceImpl(AdminAuth admin_auth) : admin_auth_(std::move(admin_auth)) {}

#if defined(NEBULA_ENABLE_STORAGE)
AdminServiceImpl::AdminServiceImpl(AdminAuth admin_auth,
                                   MySqlConnectionPool* mysql_pool,
                                   RedisClient* redis_client,
                                   AdminCleanupOptions cleanup_options,
                                   KafkaConsumerConfig kafka_consumer_config,
                                   std::vector<std::string> kafka_topics)
    : admin_auth_(std::move(admin_auth)),
      mysql_pool_(mysql_pool),
      redis_client_(redis_client),
      cleanup_options_(cleanup_options),
      kafka_consumer_config_(std::move(kafka_consumer_config)),
      kafka_topics_(std::move(kafka_topics)) {}
#endif

bool AdminServiceImpl::authorize(const grpc::ServerContext* context,
                                 const std::string& request_id,
                                 const std::string& action,
                                 const std::string& required_scope,
                                 proto::CommonResponse* response) const {
    const std::string metadata_token = RpcMetadata::extractAdminToken(context);
    AdminAuthResult result = admin_auth_.authorize(metadata_token, required_scope);
    if (result.decision == AdminAuthDecision::ALLOW) {
        LOG_INFO("admin audit allow action=" + action + " scope=" + required_scope + " principal=" + result.principal.name +
                 " request_id=" + request_id);
        return true;
    }
    const std::string reason = result.decision == AdminAuthDecision::PERMISSION_DENIED ? "admin permission denied" : "admin unauthenticated";
    LOG_WARN("admin audit deny action=" + action + " scope=" + required_scope + " reason=" + reason + " request_id=" + request_id);
    fillResponse(response, request_id, ErrorCode::AUTH_FAILED, reason);
    return false;
}

grpc::Status AdminServiceImpl::RunCleanup(grpc::ServerContext* context, const proto::RunCleanupRequest* request, proto::RunCleanupResponse* response) {
    if (!authorize(context, request->request_id(), "RunCleanup", "cleanup", response->mutable_response())) {
        return grpc::Status::OK;
    }
#if defined(NEBULA_ENABLE_STORAGE)
    if (mysql_pool_ == nullptr) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::SERVICE_UNAVAILABLE, "admin storage backend is not configured");
        return grpc::Status::OK;
    }

    auto conn = mysql_pool_->acquire();
    if (!conn) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::SERVICE_UNAVAILABLE, "failed to acquire admin mysql connection");
        return grpc::Status::OK;
    }

    const int64_t now = TimeUtil::nowMs();
    const int batch_size = cleanup_options_.cleanup_batch_size > 0 ? cleanup_options_.cleanup_batch_size : 1000;
    uint64_t cleaned_rows = 0;
    cleaned_rows += cleanupRows(*conn, "outbox_events",
                                "status=1 AND updated_at<" + std::to_string(now - cleanup_options_.outbox_published_retention_ms) +
                                    " LIMIT " + std::to_string(batch_size),
                                request->dry_run());
    cleaned_rows += cleanupRows(*conn, "offline_messages",
                                "status=1 AND updated_at<" + std::to_string(now - cleanup_options_.offline_delivered_retention_ms) +
                                    " LIMIT " + std::to_string(batch_size),
                                request->dry_run());
    cleaned_rows += cleanupRows(*conn, "friend_requests",
                                "status IN (1,2,3) AND updated_at<" + std::to_string(now - cleanup_options_.friend_request_retention_ms) +
                                    " LIMIT " + std::to_string(batch_size),
                                request->dry_run());
    cleaned_rows += cleanupRows(*conn, "message_receipts",
                                "updated_at<" + std::to_string(now - cleanup_options_.message_receipt_retention_ms) +
                                    " LIMIT " + std::to_string(batch_size),
                                request->dry_run());
    if (redis_client_ != nullptr) {
        cleaned_rows += cleanupStaleOnlineDevices(request->dry_run(), batch_size);
    }
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, request->dry_run() ? "DRY_RUN" : "OK");
    response->set_cleaned_rows(cleaned_rows);
#else
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, request->dry_run() ? "DRY_RUN" : "OK");
    response->set_cleaned_rows(0);
#endif
    return grpc::Status::OK;
}

grpc::Status AdminServiceImpl::GetSystemStats(grpc::ServerContext* context, const proto::GetSystemStatsRequest* request, proto::GetSystemStatsResponse* response) {
    if (!authorize(context, request->request_id(), "GetSystemStats", "stats", response->mutable_response())) {
        return grpc::Status::OK;
    }
#if defined(NEBULA_ENABLE_STORAGE)
    if (redis_client_ == nullptr) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::SERVICE_UNAVAILABLE, "admin redis backend is not configured");
        return grpc::Status::OK;
    }
    uint64_t online_users = 0;
    uint64_t active_connections = 0;
    for (const auto& devices_key : redis_client_->scan("nebula:user:devices:*", 1000)) {
        bool user_online = false;
        std::string prefix = "nebula:user:devices:";
        if (devices_key.rfind(prefix, 0) != 0) continue;
        std::string user_id = devices_key.substr(prefix.size());
        for (const auto& device_id : redis_client_->smembers(devices_key)) {
            if (redis_client_->exists("nebula:user:online:" + user_id + ":" + device_id) &&
                redis_client_->exists("nebula:user:conn:" + user_id + ":" + device_id)) {
                ++active_connections;
                user_online = true;
            }
        }
        if (user_online) ++online_users;
    }
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    response->set_online_users(online_users);
    response->set_active_connections(active_connections);
#else
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    response->set_online_users(0);
    response->set_active_connections(0);
#endif
    return grpc::Status::OK;
}

grpc::Status AdminServiceImpl::GetOutboxStats(grpc::ServerContext* context, const proto::GetOutboxStatsRequest* request, proto::GetOutboxStatsResponse* response) {
    if (!authorize(context, request->request_id(), "GetOutboxStats", "outbox", response->mutable_response())) {
        return grpc::Status::OK;
    }
#if defined(NEBULA_ENABLE_STORAGE)
    if (mysql_pool_ == nullptr) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::SERVICE_UNAVAILABLE, "admin storage backend is not configured");
        return grpc::Status::OK;
    }
    auto conn = mysql_pool_->acquire();
    if (!conn) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::SERVICE_UNAVAILABLE, "failed to acquire admin mysql connection");
        return grpc::Status::OK;
    }
    auto result = conn->executeQuery("SELECT status, COUNT(*) AS c FROM outbox_events GROUP BY status");
    while (result && result->next()) {
        uint64_t count = result->getUInt64("c");
        switch (result->getInt("status")) {
            case 0:
            case 4:
                response->set_pending(response->pending() + count);
                break;
            case 1:
                response->set_published(count);
                break;
            case 2:
                response->set_failed(count);
                break;
            case 3:
                response->set_dead(count);
                break;
            default:
                break;
        }
    }
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
#else
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    response->set_pending(0);
    response->set_published(0);
    response->set_failed(0);
    response->set_dead(0);
#endif
    return grpc::Status::OK;
}

grpc::Status AdminServiceImpl::GetKafkaLagInfo(grpc::ServerContext* context, const proto::GetKafkaLagInfoRequest* request, proto::GetKafkaLagInfoResponse* response) {
    if (!authorize(context, request->request_id(), "GetKafkaLagInfo", "kafka", response->mutable_response())) {
        return grpc::Status::OK;
    }
#if defined(NEBULA_ENABLE_STORAGE)
    std::string error;
    auto records = KafkaConsumer::queryLag(kafka_consumer_config_, kafka_topics_, 3000, &error);
    if (records.empty() && !error.empty()) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::SERVICE_UNAVAILABLE, "kafka lag query failed: " + error);
        return grpc::Status::OK;
    }
    std::unordered_map<std::string, int64_t> topic_lag;
    for (const auto& record : records) {
        topic_lag[record.topic] += record.lag;
    }
    for (const auto& item : topic_lag) {
        auto* lag = response->add_lags();
        lag->set_topic(item.first);
        lag->set_consumer_group(kafka_consumer_config_.group_id);
        lag->set_lag(item.second);
    }
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
#else
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "storage backend disabled");
#endif
    return grpc::Status::OK;
}

grpc::Status AdminServiceImpl::HealthCheck(grpc::ServerContext* context, const proto::HealthCheckRequest* request, proto::HealthCheckResponse* response) {
    if (!authorize(context, request->request_id(), "HealthCheck", "health", response->mutable_response())) {
        response->set_state("NOT_SERVING");
        return grpc::Status::OK;
    }
    HealthStatus status;
    status.addDependency({"admin_service", HealthState::SERVING, "process is running"});
#if defined(NEBULA_ENABLE_STORAGE)
    if (mysql_pool_ != nullptr) {
        auto conn = mysql_pool_->acquire();
        if (conn && conn->executeQuery("SELECT 1")) {
            status.addDependency({"mysql", HealthState::SERVING, "query ok"});
        } else {
            status.addDependency({"mysql", HealthState::NOT_SERVING, "query failed"});
        }
    }
    if (redis_client_ != nullptr) {
        status.addDependency({"redis", redis_client_->ping() ? HealthState::SERVING : HealthState::NOT_SERVING,
                              redis_client_->lastError()});
    }
    std::string kafka_error;
    auto lags = KafkaConsumer::queryLag(kafka_consumer_config_, kafka_topics_, 3000, &kafka_error);
    status.addDependency({"kafka", kafka_error.empty() ? HealthState::SERVING : HealthState::NOT_SERVING,
                          kafka_error.empty() ? "metadata and offsets query ok, partitions=" + std::to_string(lags.size()) : kafka_error});
#endif
    response->set_state(HealthStatus::toString(status.overall()));
    for (const auto& dependency : status.dependencies()) {
        auto* out = response->add_dependencies();
        out->set_name(dependency.name);
        out->set_state(HealthStatus::toString(dependency.state));
        out->set_detail(dependency.detail);
    }
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

#if defined(NEBULA_ENABLE_STORAGE)
uint64_t AdminServiceImpl::countRows(MySqlConnection& conn, const std::string& table, const std::string& condition) const {
    std::string count_condition = condition;
    const size_t limit_pos = count_condition.find(" LIMIT ");
    if (limit_pos != std::string::npos) {
        count_condition.erase(limit_pos);
    }
    auto result = conn.executeQuery("SELECT COUNT(*) AS c FROM " + table + " WHERE " + count_condition);
    return result && result->next() ? result->getUInt64("c") : 0;
}

uint64_t AdminServiceImpl::cleanupRows(MySqlConnection& conn,
                                       const std::string& table,
                                       const std::string& condition,
                                       bool dry_run) const {
    if (dry_run) {
        return countRows(conn, table, condition);
    }
    if (!conn.executeUpdate("DELETE FROM " + table + " WHERE " + condition)) {
        return 0;
    }
    return conn.affectedRows();
}

uint64_t AdminServiceImpl::cleanupStaleOnlineDevices(bool dry_run, int batch_size) const {
    if (redis_client_ == nullptr || batch_size <= 0) return 0;
    uint64_t cleaned = 0;
    const std::string prefix = "nebula:user:devices:";
    for (const auto& devices_key : redis_client_->scan(prefix + "*", static_cast<size_t>(batch_size))) {
        if (devices_key.rfind(prefix, 0) != 0) continue;
        const std::string user_id = devices_key.substr(prefix.size());
        for (const auto& device_id : redis_client_->smembers(devices_key)) {
            const std::string online_key = "nebula:user:online:" + user_id + ":" + device_id;
            const std::string conn_key = "nebula:user:conn:" + user_id + ":" + device_id;
            if (redis_client_->exists(online_key) && redis_client_->exists(conn_key)) continue;
            ++cleaned;
            if (!dry_run) {
                redis_client_->del(online_key);
                redis_client_->del(conn_key);
                redis_client_->srem(devices_key, device_id);
            }
            if (cleaned >= static_cast<uint64_t>(batch_size)) return cleaned;
        }
    }
    return cleaned;
}
#endif

}  // namespace nebula
