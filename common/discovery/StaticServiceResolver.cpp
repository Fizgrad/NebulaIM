#include "common/discovery/StaticServiceResolver.h"

#include "common/config/Config.h"

#include <stdexcept>
#include <utility>

namespace nebula {

void StaticServiceResolver::addInstance(const ServiceInstance& instance) {
    if (instance.service_name.empty() || instance.address.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    instances_[instance.service_name].push_back(instance);
}

void StaticServiceResolver::addInstance(const std::string& service_name, const std::string& address, const std::string& instance_id) {
    ServiceInstance instance;
    instance.service_name = service_name;
    instance.instance_id = instance_id.empty() ? service_name + "-static-1" : instance_id;
    instance.address = address;
    addInstance(instance);
}

bool StaticServiceResolver::loadFromConfig(const Config& config) {
    addInstance("user_service", config.getString("user_service.addr", "127.0.0.1:50051"));
    addInstance("message_service", config.getString("message_service.addr", "127.0.0.1:50052"));
    addInstance("relation_service", config.getString("relation_service.addr", "127.0.0.1:50053"));
    addInstance("conversation_service", config.getString("conversation_service.addr", "127.0.0.1:50056"));
    addInstance("push_service", config.getString("push_service.addr", "127.0.0.1:50054"));
    addInstance("admin_service", config.getString("admin_service.addr", "127.0.0.1:50057"));

    std::string gateway_id = config.getString("gateway.default_id", "gateway-1");
    addInstance("gateway", config.getString("gateway." + gateway_id + ".addr", "127.0.0.1:50055"), gateway_id);
    return true;
}

std::vector<ServiceInstance> StaticServiceResolver::resolve(const std::string& service_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = instances_.find(service_name);
    if (it == instances_.end()) return {};
    return it->second;
}

ServiceInstance StaticServiceResolver::pick(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = instances_.find(service_name);
    if (it == instances_.end() || it->second.empty()) {
        throw std::runtime_error("service not found: " + service_name);
    }
    auto& list = it->second;
    size_t& idx = next_[service_name];
    for (size_t i = 0; i < list.size(); ++i) {
        ServiceInstance instance = list[(idx + i) % list.size()];
        if (instance.healthy) {
            idx = (idx + i + 1) % list.size();
            return instance;
        }
    }
    throw std::runtime_error("no healthy service instance: " + service_name);
}

}  // namespace nebula
