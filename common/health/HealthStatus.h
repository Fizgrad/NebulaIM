#pragma once

#include <string>
#include <vector>

namespace nebula {

enum class HealthState {
    SERVING,
    DEGRADED,
    NOT_SERVING
};

struct DependencyHealth {
    std::string name;
    HealthState state = HealthState::SERVING;
    std::string detail;
};

class HealthStatus {
public:
    void addDependency(DependencyHealth dependency);
    HealthState overall() const;
    const std::vector<DependencyHealth>& dependencies() const;

    static std::string toString(HealthState state);

private:
    std::vector<DependencyHealth> dependencies_;
};

}  // namespace nebula
