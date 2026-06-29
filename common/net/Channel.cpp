#include "common/net/Channel.h"

#include "common/net/EventLoop.h"

#include <sys/epoll.h>

namespace nebula {

namespace {
constexpr int kNew = -1;
constexpr uint32_t kNoneEvent = 0;
constexpr uint32_t kReadEvent = EPOLLIN | EPOLLPRI;
constexpr uint32_t kWriteEvent = EPOLLOUT;
}

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0),
      revents_(0),
      index_(kNew) {}

Channel::~Channel() = default;

void Channel::handleEvent() {
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if (close_callback_) {
            close_callback_();
        }
    }

    if (revents_ & EPOLLERR) {
        if (error_callback_) {
            error_callback_();
        }
    }

    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if (read_callback_) {
            read_callback_();
        }
    }

    if (revents_ & EPOLLOUT) {
        if (write_callback_) {
            write_callback_();
        }
    }
}

void Channel::setReadCallback(EventCallback cb) {
    read_callback_ = std::move(cb);
}

void Channel::setWriteCallback(EventCallback cb) {
    write_callback_ = std::move(cb);
}

void Channel::setCloseCallback(EventCallback cb) {
    close_callback_ = std::move(cb);
}

void Channel::setErrorCallback(EventCallback cb) {
    error_callback_ = std::move(cb);
}

int Channel::fd() const {
    return fd_;
}

uint32_t Channel::events() const {
    return events_;
}

void Channel::setRevents(uint32_t revents) {
    revents_ = revents;
}

bool Channel::isNoneEvent() const {
    return events_ == kNoneEvent;
}

bool Channel::isWriting() const {
    return (events_ & kWriteEvent) != 0;
}

void Channel::enableReading() {
    events_ |= kReadEvent;
    update();
}

void Channel::enableWriting() {
    events_ |= kWriteEvent;
    update();
}

void Channel::disableWriting() {
    events_ &= ~kWriteEvent;
    update();
}

void Channel::disableAll() {
    events_ = kNoneEvent;
    update();
}

void Channel::remove() {
    loop_->removeChannel(this);
}

int Channel::index() const {
    return index_;
}

void Channel::setIndex(int index) {
    index_ = index;
}

EventLoop* Channel::ownerLoop() {
    return loop_;
}

void Channel::update() {
    loop_->updateChannel(this);
}

}  // namespace nebula
