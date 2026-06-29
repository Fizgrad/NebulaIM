#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace nebula {

enum class TraceSpanKind {
    INTERNAL,
    SERVER,
    CLIENT,
    PRODUCER,
    CONSUMER
};

struct CompletedSpan {
    std::string trace_id;
    std::string span_id;
    std::string parent_span_id;
    std::string name;
    TraceSpanKind kind = TraceSpanKind::INTERNAL;
    int64_t start_time_unix_nano = 0;
    int64_t end_time_unix_nano = 0;
    std::map<std::string, std::string> attributes;
    std::string status_code = "STATUS_CODE_OK";
    std::string status_message;
};

class TraceSpan {
public:
    TraceSpan(std::string name, TraceSpanKind kind = TraceSpanKind::INTERNAL);
    ~TraceSpan();

    TraceSpan(const TraceSpan&) = delete;
    TraceSpan& operator=(const TraceSpan&) = delete;

    TraceSpan(TraceSpan&& other) noexcept;
    TraceSpan& operator=(TraceSpan&& other) noexcept;

    void setAttribute(std::string key, std::string value);
    void setError(std::string message);
    void finish();

    const std::string& traceId() const;
    const std::string& spanId() const;

private:
    void moveFrom(TraceSpan&& other) noexcept;

private:
    bool active_ = false;
    std::string previous_span_id_;
    CompletedSpan span_;
};

}  // namespace nebula
