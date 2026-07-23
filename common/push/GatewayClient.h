#pragma once

#include "common/circuitbreaker/CircuitBreaker.h"
#include "common/rpc/GrpcTlsCredentials.h"
#include "gateway.grpc.pb.h"

#include <memory>
#include <string>

namespace nebula {

class GatewayClient {
public:
    explicit GatewayClient(const std::string& address);
    GatewayClient(const std::string& address, const GrpcTlsConfig& tls_config);

    bool deliverToConnection(const std::string& request_id,
                             uint64_t user_id,
                             const std::string& connection_id,
                             const proto::MessageData& message);

    bool kickUser(const std::string& request_id, uint64_t user_id, const std::string& reason);
    bool kickConnection(const std::string& request_id,
                        uint64_t user_id,
                        const std::string& connection_id,
                        const std::string& device_id,
                        const std::string& reason);

    bool getOnlineStatus(const std::string& request_id,
                         uint64_t user_id,
                         bool* online,
                         std::string* gateway_id,
                         std::string* connection_id);

private:
    CircuitBreaker breaker_;
    std::unique_ptr<proto::GatewayService::Stub> stub_;
};

}  // namespace nebula
