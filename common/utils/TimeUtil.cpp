#include "common/utils/TimeUtil.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace nebula {

int64_t TimeUtil::nowMs() {
    const auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

int64_t TimeUtil::nowUs() {
    const auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
}

std::string TimeUtil::nowString() {
    const auto now = std::chrono::system_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    const std::time_t time = std::chrono::system_clock::to_time_t(now);

    std::tm tm_value{};
#if defined(_WIN32)
    localtime_s(&tm_value, &time);
#else
    localtime_r(&time, &tm_value);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_value, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setw(3) << std::setfill('0') << ms.count();
    return oss.str();
}

}  // namespace nebula
