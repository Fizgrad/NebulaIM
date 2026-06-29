#include "common/trace/TraceSpan.h"

#include "common/trace/TraceContext.h"
#include "common/trace/TraceId.h"
#include "common/trace/TraceManager.h"
#include "common/utils/TimeUtil.h"

#include <utility>

namespace nebula {

namespace {
int64_t nowNs() {
    return TimeUtil::nowMs() * 1000000LL;
}
}

TraceSpan::TraceSpan(std::string name, TraceSpanKind kind) {
    active_ = true;
    previous_span_id_ = TraceContext::spanId();
    span_.trace_id = TraceContext::ensureTraceId();
    span_.span_id = TraceId::generateSpanId();
    span_.parent_span_id = previous_span_id_;
    span_.name = std::move(name);
    span_.kind = kind;
    span_.start_time_unix_nano = nowNs();
    TraceContext::setSpanId(span_.span_id);
}

TraceSpan::~TraceSpan() {
    finish();
}

TraceSpan::TraceSpan(TraceSpan&& other) noexcept {
    moveFrom(std::move(other));
}

TraceSpan& TraceSpan::operator=(TraceSpan&& other) noexcept {
    if (this != &other) {
        finish();
        moveFrom(std::move(other));
    }
    return *this;
}

void TraceSpan::setAttribute(std::string key, std::string value) {
    if (!active_) return;
    span_.attributes[std::move(key)] = std::move(value);
}

void TraceSpan::setError(std::string message) {
    if (!active_) return;
    span_.status_code = "STATUS_CODE_ERROR";
    span_.status_message = std::move(message);
}

void TraceSpan::finish() {
    if (!active_) return;
    span_.end_time_unix_nano = nowNs();
    TraceContext::setSpanId(previous_span_id_);
    TraceManager::instance().record(std::move(span_));
    active_ = false;
}

const std::string& TraceSpan::traceId() const {
    return span_.trace_id;
}

const std::string& TraceSpan::spanId() const {
    return span_.span_id;
}

void TraceSpan::moveFrom(TraceSpan&& other) noexcept {
    active_ = other.active_;
    previous_span_id_ = std::move(other.previous_span_id_);
    span_ = std::move(other.span_);
    other.active_ = false;
}

}  // namespace nebula
