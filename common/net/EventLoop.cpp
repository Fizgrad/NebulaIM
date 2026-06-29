#include "common/net/EventLoop.h"

#include "common/log/Logger.h"
#include "common/net/Channel.h"
#include "common/net/EpollPoller.h"
#include "common/net/TimerQueue.h"
#include "common/utils/TimeUtil.h"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <sys/eventfd.h>
#include <unistd.h>

namespace nebula {

namespace {

int createEventfd() {
    int fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (fd < 0) {
        throw std::runtime_error(std::string("eventfd failed: ") + std::strerror(errno));
    }
    return fd;
}

}  // namespace

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      thread_id_(std::this_thread::get_id()),
      poller_(std::make_unique<EpollPoller>()),
      timer_queue_(nullptr),
      wakeup_fd_(createEventfd()),
      wakeup_channel_(std::make_unique<Channel>(this, wakeup_fd_)),
      calling_pending_functors_(false) {
    wakeup_channel_->setReadCallback([this]() { handleWakeupRead(); });
    wakeup_channel_->enableReading();
    timer_queue_ = std::make_unique<TimerQueue>(this);
}

EventLoop::~EventLoop() {
    wakeup_channel_->disableAll();
    wakeup_channel_->remove();
    ::close(wakeup_fd_);
}

void EventLoop::loop() {
    assert(!looping_);
    assertInLoopThread();
    looping_ = true;
    quit_ = false;

    while (!quit_) {
        EpollPoller::ChannelList active_channels;
        poller_->poll(10000, &active_channels);
        for (Channel* channel : active_channels) {
            channel->handleEvent();
        }
        doPendingFunctors();
    }

    looping_ = false;
}

void EventLoop::quit() {
    quit_ = true;
    if (!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_functors_.push_back(std::move(cb));
    }

    if (!isInLoopThread() || calling_pending_functors_) {
        wakeup();
    }
}

void EventLoop::updateChannel(Channel* channel) {
    assertInLoopThread();
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel) {
    assertInLoopThread();
    poller_->removeChannel(channel);
}

bool EventLoop::isInLoopThread() const {
    return thread_id_ == std::this_thread::get_id();
}

void EventLoop::assertInLoopThread() const {
    if (!isInLoopThread()) {
        throw std::runtime_error("EventLoop used from non-owner thread");
    }
}

void EventLoop::runAt(int64_t timestamp_ms, Functor cb) {
    timer_queue_->addTimer(std::move(cb), timestamp_ms, 0);
}

void EventLoop::runAfter(int64_t delay_ms, Functor cb) {
    runAt(TimeUtil::nowMs() + delay_ms, std::move(cb));
}

void EventLoop::runEvery(int64_t interval_ms, Functor cb) {
    timer_queue_->addTimer(std::move(cb), TimeUtil::nowMs() + interval_ms, interval_ms);
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = ::write(wakeup_fd_, &one, sizeof(one));
    if (n != sizeof(one) && errno != EAGAIN) {
        LOG_ERROR(std::string("eventfd write failed: ") + std::strerror(errno));
    }
}

void EventLoop::handleWakeupRead() {
    uint64_t one = 0;
    ssize_t n = ::read(wakeup_fd_, &one, sizeof(one));
    if (n != sizeof(one) && errno != EAGAIN) {
        LOG_ERROR(std::string("eventfd read failed: ") + std::strerror(errno));
    }
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    calling_pending_functors_ = true;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pending_functors_);
    }

    for (const Functor& functor : functors) {
        functor();
    }

    calling_pending_functors_ = false;
}

}  // namespace nebula
