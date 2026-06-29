#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nebula {

struct ServiceInstance {
    std::string service_name;
    std::string instance_id;
    std::string address;
    int weight = 100;
    bool healthy = true;
};

class ServiceResolver {
public:
    virtual ~ServiceResolver() = default;

    virtual std::vector<ServiceInstance> resolve(const std::string& service_name) const = 0;
    virtual ServiceInstance pick(const std::string& service_name) = 0;
};

}  // namespace nebula
