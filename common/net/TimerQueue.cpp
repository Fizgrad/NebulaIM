#include "common/net/TimerQueue.h"

#include "common/log/Logger.h"
#include "common/net/Channel.h"
#include "common/net/EventLoop.h"
#include "common/net/Timer.h"
#include "common/utils/TimeUtil.h"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <sys/timerfd.h>
#include <unistd.h>

namespace nebula {

namespace {

int createTimerfd() {
    int fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (fd < 0) {
        throw std::runtime_error(std::string("timerfd_create failed: ") + std::strerror(errno));
    }
    return fd;
}

struct timespec howMuchTimeFromNow(int64_t timestamp_ms) {
    int64_t diff_ms = timestamp_ms - TimeUtil::nowMs();
    if (diff_ms < 1) {
        diff_ms = 1;
    }

    struct timespec ts{};
    ts.tv_sec = static_cast<time_t>(diff_ms / 1000);
    ts.tv_nsec = static_cast<long>((diff_ms % 1000) * 1000 * 1000);
    return ts;
}

void resetTimerfd(int timerfd, int64_t expiration_ms) {
    struct itimerspec new_value{};
    new_value.it_value = howMuchTimeFromNow(expiration_ms);
    if (::timerfd_settime(timerfd, 0, &new_value, nullptr) < 0) {
        LOG_ERROR(std::string("timerfd_settime failed: ") + std::strerror(errno));
    }
}

void readTimerfd(int timerfd) {
    uint64_t expirations = 0;
    ssize_t n = ::read(timerfd, &expirations, sizeof(expirations));
    if (n != sizeof(expirations) && errno != EAGAIN) {
        LOG_ERROR(std::string("timerfd read failed: ") + std::strerror(errno));
    }
}

}  // namespace

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop),
      timerfd_(createTimerfd()),
      timerfd_channel_(std::make_unique<Channel>(loop, timerfd_)) {
    timerfd_channel_->setReadCallback([this]() { handleRead(); });
    timerfd_channel_->enableReading();
}

TimerQueue::~TimerQueue() {
    timerfd_channel_->disableAll();
    timerfd_channel_->remove();
    ::close(timerfd_);
}

void TimerQueue::addTimer(TimerCallback cb, int64_t timestamp_ms, int64_t interval_ms) {
    auto timer = std::make_shared<Timer>(std::move(cb), timestamp_ms, interval_ms);
    loop_->runInLoop([this, timer]() { addTimerInLoop(timer); });
}

void TimerQueue::addTimerInLoop(std::shared_ptr<Timer> timer) {
    loop_->assertInLoopThread();
    bool earliest_changed = timers_.empty() || timer->expiration() < timers_.begin()->first;
    timers_.insert({timer->expiration(), timer});
    if (earliest_changed) {
        resetTimerfd(timerfd_, timer->expiration());
    }
}

void TimerQueue::handleRead() {
    loop_->assertInLoopThread();
    readTimerfd(timerfd_);

    const int64_t now = TimeUtil::nowMs();
    std::vector<std::shared_ptr<Timer>> expired = getExpired(now);
    for (const auto& timer : expired) {
        timer->run();
    }
    reset(expired, now);
}

std::vector<std::shared_ptr<Timer>> TimerQueue::getExpired(int64_t now_ms) {
    std::vector<std::shared_ptr<Timer>> expired;
    auto end = timers_.lower_bound({now_ms + 1, std::shared_ptr<Timer>()});
    for (auto it = timers_.begin(); it != end; ++it) {
        expired.push_back(it->second);
    }
    timers_.erase(timers_.begin(), end);
    return expired;
}

void TimerQueue::reset(const std::vector<std::shared_ptr<Timer>>& expired, int64_t now_ms) {
    for (const auto& timer : expired) {
        if (timer->repeat()) {
            timer->restart(now_ms);
            timers_.insert({timer->expiration(), timer});
        }
    }

    if (!timers_.empty()) {
        resetTimerfd(timerfd_, timers_.begin()->second->expiration());
    }
}

}  // namespace nebula
