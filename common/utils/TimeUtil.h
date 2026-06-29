#pragma once

#include <string>

namespace nebula {

class TimeUtil {
public:
    static int64_t nowMs();
    static int64_t nowUs();
    static std::string nowString();
};

}  // namespace nebula
