#include "common/trace/TraceContext.h"
#include "common/trace/TraceId.h"
#include "common/trace/TraceSpan.h"

#include <cassert>
#include <iostream>

int main() {
    std::string id = nebula::TraceId::generate();
    assert(nebula::TraceId::isValid(id));
    nebula::TraceContext::setTraceId(id);
    assert(nebula::TraceContext::traceId() == id);
    {
        nebula::TraceContext::Scope scope("abcdef1234567890");
        assert(nebula::TraceContext::traceId() == "abcdef1234567890");
        nebula::TraceSpan span("test.span");
        assert(nebula::TraceId::isValidSpanId(span.spanId()));
        assert(nebula::TraceContext::spanId() == span.spanId());
    }
    assert(nebula::TraceContext::traceId() == id);
    assert(nebula::TraceContext::spanId().empty());
    assert(nebula::TraceId::isValidSpanId(nebula::TraceId::generateSpanId()));
    std::cout << "test_trace_context passed\n";
    return 0;
}
