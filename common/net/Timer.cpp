#include "common/net/Timer.h"

namespace nebula {

Timer::Timer(TimerCallback cb, int64_t expiration_ms, int64_t interval_ms)
    : callback_(std::move(cb)),
      expiration_ms_(expiration_ms),
      interval_ms_(interval_ms),
      repeat_(interval_ms > 0) {}

void Timer::run() const {
    if (callback_) {
        callback_();
    }
}

int64_t Timer::expiration() const {
    return expiration_ms_;
}

bool Timer::repeat() const {
    return repeat_;
}

void Timer::restart(int64_t now_ms) {
    if (repeat_) {
        expiration_ms_ = now_ms + interval_ms_;
    }
}

}  // namespace nebula
