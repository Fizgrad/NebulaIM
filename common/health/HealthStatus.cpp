#include "common/health/HealthStatus.h"

#include <utility>

namespace nebula {

void HealthStatus::addDependency(DependencyHealth dependency) {
    dependencies_.push_back(std::move(dependency));
}

HealthState HealthStatus::overall() const {
    HealthState result = HealthState::SERVING;
    for (const auto& dependency : dependencies_) {
        if (dependency.state == HealthState::NOT_SERVING) return HealthState::NOT_SERVING;
        if (dependency.state == HealthState::DEGRADED) result = HealthState::DEGRADED;
    }
    return result;
}

const std::vector<DependencyHealth>& HealthStatus::dependencies() const {
    return dependencies_;
}

std::string HealthStatus::toString(HealthState state) {
    switch (state) {
        case HealthState::SERVING: return "SERVING";
        case HealthState::DEGRADED: return "DEGRADED";
        case HealthState::NOT_SERVING: return "NOT_SERVING";
    }
    return "UNKNOWN";
}

}  // namespace nebula
