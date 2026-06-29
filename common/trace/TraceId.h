#pragma once

#include <string>

namespace nebula {

class TraceId {
public:
    static std::string generate();
    static std::string generateSpanId();
    static bool isValid(const std::string& trace_id);
    static bool isValidSpanId(const std::string& span_id);
};

}  // namespace nebula
