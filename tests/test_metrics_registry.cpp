#include "common/monitor/MetricsRegistry.h"

#include <cassert>
#include <string>

int main() {
    auto& registry = nebula::MetricsRegistry::instance();
    auto& counter = registry.counter("test_counter_total", "test counter");
    counter.inc();
    counter.inc(2);
    assert(counter.value() >= 3);

    auto& gauge = registry.gauge("test_gauge", "test gauge");
    gauge.set(10);
    gauge.inc(5);
    gauge.dec(3);
    assert(gauge.value() == 12);

    auto& hist = registry.histogram("test_latency_ms", "latency", {1, 10, 100});
    hist.observe(5);
    std::string rendered = registry.renderPrometheus();
    assert(rendered.find("test_counter_total") != std::string::npos);
    assert(rendered.find("test_gauge") != std::string::npos);
    assert(rendered.find("test_latency_ms_bucket") != std::string::npos);
    return 0;
}
