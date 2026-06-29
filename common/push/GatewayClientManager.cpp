#include "common/push/GatewayClientManager.h"

#include "common/discovery/ServiceDiscovery.h"

namespace nebula {

GatewayClientManager::GatewayClientManager() = default;

void GatewayClientManager::addGateway(const std::string& gateway_id, const std::string& address) {
    addGateway(gateway_id, address, GrpcTlsConfig{});
}

void GatewayClientManager::addGateway(const std::string& gateway_id, const std::string& address, const GrpcTlsConfig& tls_config) {
    clients_[gateway_id] = std::make_unique<GatewayClient>(address, tls_config);
}

bool GatewayClientManager::initFromResolver(ServiceResolver& resolver) {
    return initFromResolver(resolver, GrpcTlsConfig{});
}

bool GatewayClientManager::initFromResolver(ServiceResolver& resolver, const GrpcTlsConfig& tls_config) {
    auto instances = resolver.resolve("gateway");
    for (const auto& instance : instances) {
        if (instance.healthy) {
            addGateway(instance.instance_id.empty() ? instance.service_name : instance.instance_id,
                       instance.address,
                       tls_config);
        }
    }
    return !clients_.empty();
}

GatewayClient* GatewayClientManager::getClient(const std::string& gateway_id) {
    auto it = clients_.find(gateway_id);
    return it == clients_.end() ? nullptr : it->second.get();
}

bool GatewayClientManager::empty() const { return clients_.empty(); }

}  // namespace nebula
