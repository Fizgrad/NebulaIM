#pragma once

#include "common/push/GatewayClient.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace nebula {

class GatewayClientManager {
public:
    GatewayClientManager();

    void addGateway(const std::string& gateway_id, const std::string& address);
    GatewayClient* getClient(const std::string& gateway_id);
    bool empty() const;

private:
    std::unordered_map<std::string, std::unique_ptr<GatewayClient>> clients_;
};

}  // namespace nebula
