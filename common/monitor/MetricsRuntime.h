#pragma once

#include "common/monitor/MetricsHttpServer.h"

#include <memory>
#include <string>

namespace nebula {

std::unique_ptr<MetricsHttpServer> startMetricsServerFromConfig(const std::string& config_path,
                                                                const std::string& service_key,
                                                                int default_port);

}  // namespace nebula
