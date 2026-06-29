#pragma once

#include "AdminServiceContext.h"
#include "admin.grpc.pb.h"

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

namespace nebula {

#if defined(NEBULA_ENABLE_STORAGE)
class MySqlConnection;
#endif

class AdminServiceImpl final : public proto::AdminService::Service {
public:
    explicit AdminServiceImpl(AdminAuth admin_auth = AdminAuth{});
#if defined(NEBULA_ENABLE_STORAGE)
    AdminServiceImpl(AdminAuth admin_auth,
                     AdminRuntimeConfig runtime_config,
                     MySqlConnectionPool* mysql_pool,
                     RedisClient* redis_client,
                     AdminCleanupOptions cleanup_options = {},
                     KafkaConsumerConfig kafka_consumer_config = {},
                     std::vector<std::string> kafka_topics = {});
#endif

    grpc::Status RunCleanup(grpc::ServerContext* context,
                            const proto::RunCleanupRequest* request,
                            proto::RunCleanupResponse* response) override;
    grpc::Status GetSystemStats(grpc::ServerContext* context,
                                const proto::GetSystemStatsRequest* request,
                                proto::GetSystemStatsResponse* response) override;
    grpc::Status GetOutboxStats(grpc::ServerContext* context,
                                const proto::GetOutboxStatsRequest* request,
                                proto::GetOutboxStatsResponse* response) override;
    grpc::Status GetKafkaLagInfo(grpc::ServerContext* context,
                                 const proto::GetKafkaLagInfoRequest* request,
                                 proto::GetKafkaLagInfoResponse* response) override;
    grpc::Status HealthCheck(grpc::ServerContext* context,
                             const proto::HealthCheckRequest* request,
                             proto::HealthCheckResponse* response) override;
    grpc::Status ValidateConfig(grpc::ServerContext* context,
                                const proto::ValidateConfigRequest* request,
                                proto::ValidateConfigResponse* response) override;
    grpc::Status GetServiceOverview(grpc::ServerContext* context,
                                    const proto::GetServiceOverviewRequest* request,
                                    proto::GetServiceOverviewResponse* response) override;
    grpc::Status ListAuditEvents(grpc::ServerContext* context,
                                 const proto::ListAuditEventsRequest* request,
                                 proto::ListAuditEventsResponse* response) override;

private:
    struct AuditEvent {
        int64_t timestamp_ms = 0;
        std::string request_id;
        std::string principal;
        std::string action;
        std::string scope;
        std::string decision;
        std::string detail;
    };

    bool authorize(const grpc::ServerContext* context,
                   const std::string& request_id,
                   const std::string& action,
                   const std::string& required_scope,
                   proto::CommonResponse* response) const;
    void appendAudit(AuditEvent event) const;
    std::vector<AuditEvent> auditEvents(size_t limit) const;
    void addConfigIssue(proto::ValidateConfigResponse* response,
                        const std::string& severity,
                        const std::string& key,
                        const std::string& message) const;
    bool canConnectAddress(const std::string& address, int timeout_ms, std::string* detail) const;
#if defined(NEBULA_ENABLE_STORAGE)
    uint64_t countRows(MySqlConnection& conn, const std::string& table, const std::string& condition) const;
    uint64_t cleanupRows(MySqlConnection& conn,
                         const std::string& table,
                         const std::string& condition,
                         bool dry_run) const;
    uint64_t cleanupStaleOnlineDevices(bool dry_run, int batch_size) const;
#endif

private:
    AdminAuth admin_auth_;
    AdminRuntimeConfig runtime_config_;
    mutable std::mutex audit_mutex_;
    mutable std::deque<AuditEvent> audit_events_;
    size_t max_audit_events_ = 1024;
#if defined(NEBULA_ENABLE_STORAGE)
    MySqlConnectionPool* mysql_pool_ = nullptr;
    RedisClient* redis_client_ = nullptr;
    AdminCleanupOptions cleanup_options_;
    KafkaConsumerConfig kafka_consumer_config_;
    std::vector<std::string> kafka_topics_;
#endif
};

}  // namespace nebula
