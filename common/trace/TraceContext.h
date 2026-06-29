#pragma once

#include <string>

namespace nebula {

class TraceContext {
public:
    class Scope {
    public:
        explicit Scope(std::string trace_id);
        ~Scope();

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        std::string previous_;
    };

    static void setTraceId(const std::string& trace_id);
    static std::string traceId();
    static void clear();
    static std::string ensureTraceId(const std::string& candidate = "");
};

}  // namespace nebula
