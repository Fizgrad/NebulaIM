#include "common/net/EpollPoller.h"

#include "common/log/Logger.h"
#include "common/net/Channel.h"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <sys/epoll.h>
#include <unistd.h>

namespace nebula {

namespace {
constexpr int kNew = -1;
constexpr int kAdded = 1;
constexpr int kDeleted = 2;
constexpr int kInitialEventListSize = 16;
}

EpollPoller::EpollPoller()
    : epoll_fd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitialEventListSize) {
    if (epoll_fd_ < 0) {
        throw std::runtime_error(std::string("epoll_create1 failed: ") + std::strerror(errno));
    }
}

EpollPoller::~EpollPoller() {
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
    }
}

void EpollPoller::poll(int timeout_ms, ChannelList* active_channels) {
    int num_events = ::epoll_wait(epoll_fd_, events_.data(), static_cast<int>(events_.size()), timeout_ms);
    if (num_events > 0) {
        fillActiveChannels(num_events, active_channels);
        if (static_cast<size_t>(num_events) == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    } else if (num_events < 0 && errno != EINTR) {
        LOG_ERROR(std::string("epoll_wait failed: ") + std::strerror(errno));
    }
}

void EpollPoller::updateChannel(Channel* channel) {
    const int index = channel->index();
    const int fd = channel->fd();
    if (index == kNew || index == kDeleted) {
        channels_[fd] = channel;
        channel->setIndex(kAdded);
        update(EPOLL_CTL_ADD, channel);
    } else {
        if (channel->isNoneEvent()) {
            update(EPOLL_CTL_DEL, channel);
            channel->setIndex(kDeleted);
        } else {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EpollPoller::removeChannel(Channel* channel) {
    const int fd = channel->fd();
    channels_.erase(fd);
    if (channel->index() == kAdded) {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->setIndex(kNew);
}

void EpollPoller::update(int operation, Channel* channel) {
    epoll_event event{};
    event.events = channel->events();
    event.data.ptr = channel;
    if (::epoll_ctl(epoll_fd_, operation, channel->fd(), &event) < 0) {
        LOG_ERROR(std::string("epoll_ctl failed fd=") + std::to_string(channel->fd()) + ": " + std::strerror(errno));
    }
}

void EpollPoller::fillActiveChannels(int num_events, ChannelList* active_channels) const {
    for (int i = 0; i < num_events; ++i) {
        auto* channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->setRevents(events_[i].events);
        active_channels->push_back(channel);
    }
}

}  // namespace nebula
