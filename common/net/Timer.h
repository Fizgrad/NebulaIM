#pragma once

#include <cstdint>
#include <functional>

namespace nebula {

class Timer {
public:
    using TimerCallback = std::function<void()>;

    Timer(TimerCallback cb, int64_t expiration_ms, int64_t interval_ms);

    void run() const;
    int64_t expiration() const;
    bool repeat() const;
    void restart(int64_t now_ms);

private:
    TimerCallback callback_;
    int64_t expiration_ms_;
    int64_t interval_ms_;
    bool repeat_;
};

}  // namespace nebula
