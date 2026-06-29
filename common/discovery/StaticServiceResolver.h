#pragma once

#include "common/discovery/ServiceDiscovery.h"

#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

namespace nebula {

class Config;

class StaticServiceResolver final : public ServiceResolver {
public:
    StaticServiceResolver() = default;

    void addInstance(const ServiceInstance& instance);
    void addInstance(const std::string& service_name, const std::string& address, const std::string& instance_id = "");
    bool loadFromConfig(const Config& config);

    std::vector<ServiceInstance> resolve(const std::string& service_name) const override;
    ServiceInstance pick(const std::string& service_name) override;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::vector<ServiceInstance>> instances_;
    std::unordered_map<std::string, size_t> next_;
};

}  // namespace nebula
