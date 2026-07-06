#include "common/monitor/MetricsRuntime.h"

#include "common/config/Config.h"
#include "common/log/Logger.h"

namespace nebula {

std::unique_ptr<MetricsHttpServer> startMetricsServerFromConfig(const std::string& config_path,
                                                                const std::string& service_key,
                                                                int default_port) {
    Config config;
    if (!config.loadFromFile(config_path)) {
        LOG_WARN("metrics disabled: failed to load config " + config_path);
        return nullptr;
    }
    if (!config.getBool("metrics.enabled", true)) {
        return nullptr;
    }

    const std::string host = config.getString("metrics.host", "127.0.0.1");
    const int port = config.getInt("metrics." + service_key + ".port", default_port);
    auto server = std::make_unique<MetricsHttpServer>(host, port);
    if (!server->start()) {
        LOG_WARN("failed to start metrics server on " + host + ":" + std::to_string(port));
        return nullptr;
    }
    LOG_INFO("metrics server listening on " + host + ":" + std::to_string(port));
    return server;
}

}  // namespace nebula
