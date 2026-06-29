#include "common/trace/TraceContext.h"

#include "common/trace/TraceId.h"

namespace nebula {

namespace {
thread_local std::string tls_trace_id;
}

TraceContext::Scope::Scope(std::string trace_id) : previous_(tls_trace_id) {
    tls_trace_id = std::move(trace_id);
}

TraceContext::Scope::~Scope() {
    tls_trace_id = std::move(previous_);
}

void TraceContext::setTraceId(const std::string& trace_id) {
    tls_trace_id = trace_id;
}

std::string TraceContext::traceId() {
    return tls_trace_id;
}

void TraceContext::clear() {
    tls_trace_id.clear();
}

std::string TraceContext::ensureTraceId(const std::string& candidate) {
    if (TraceId::isValid(candidate)) {
        tls_trace_id = candidate;
    } else if (tls_trace_id.empty()) {
        tls_trace_id = TraceId::generate();
    }
    return tls_trace_id;
}

}  // namespace nebula
