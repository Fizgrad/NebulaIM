#include "common/push/GatewayClientManager.h"

namespace nebula {

GatewayClientManager::GatewayClientManager() = default;

void GatewayClientManager::addGateway(const std::string& gateway_id, const std::string& address) {
    clients_[gateway_id] = std::make_unique<GatewayClient>(address);
}

GatewayClient* GatewayClientManager::getClient(const std::string& gateway_id) {
    auto it = clients_.find(gateway_id);
    return it == clients_.end() ? nullptr : it->second.get();
}

bool GatewayClientManager::empty() const { return clients_.empty(); }

}  // namespace nebula
