#pragma once

#include "common/push/GatewayClient.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace nebula {

class ServiceResolver;

class GatewayClientManager {
public:
    GatewayClientManager();

    void addGateway(const std::string& gateway_id, const std::string& address);
    void addGateway(const std::string& gateway_id, const std::string& address, const GrpcTlsConfig& tls_config);
    bool initFromResolver(ServiceResolver& resolver);
    bool initFromResolver(ServiceResolver& resolver, const GrpcTlsConfig& tls_config);
    GatewayClient* getClient(const std::string& gateway_id);
    bool empty() const;

private:
    std::unordered_map<std::string, std::unique_ptr<GatewayClient>> clients_;
};

}  // namespace nebula
