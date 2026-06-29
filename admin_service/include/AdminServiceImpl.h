#pragma once

#include "AdminServiceContext.h"
#include "admin.grpc.pb.h"

#include <cstdint>
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
                     MySqlConnectionPool* mysql_pool,
                     RedisClient* redis_client,
                     AdminCleanupOptions cleanup_options = {});
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

private:
    bool authorize(const grpc::ServerContext* context,
                   const std::string& request_id,
                   const std::string& action,
                   const std::string& required_scope,
                   proto::CommonResponse* response) const;
#if defined(NEBULA_ENABLE_STORAGE)
    uint64_t countRows(MySqlConnection& conn, const std::string& table, const std::string& condition) const;
    uint64_t cleanupRows(MySqlConnection& conn,
                         const std::string& table,
                         const std::string& condition,
                         bool dry_run) const;
#endif

private:
    AdminAuth admin_auth_;
#if defined(NEBULA_ENABLE_STORAGE)
    MySqlConnectionPool* mysql_pool_ = nullptr;
    RedisClient* redis_client_ = nullptr;
    AdminCleanupOptions cleanup_options_;
#endif
};

}  // namespace nebula
