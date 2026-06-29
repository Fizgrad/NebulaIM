#pragma once

#include <string>

namespace nebula {

class TraceId {
public:
    static std::string generate();
    static bool isValid(const std::string& trace_id);
};

}  // namespace nebula
