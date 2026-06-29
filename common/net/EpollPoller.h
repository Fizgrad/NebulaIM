#pragma once

#include <unordered_map>
#include <vector>

struct epoll_event;

namespace nebula {

class Channel;

class EpollPoller {
public:
    using ChannelList = std::vector<Channel*>;

    EpollPoller();
    ~EpollPoller();

    EpollPoller(const EpollPoller&) = delete;
    EpollPoller& operator=(const EpollPoller&) = delete;

    void poll(int timeout_ms, ChannelList* active_channels);

    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

private:
    void update(int operation, Channel* channel);
    void fillActiveChannels(int num_events, ChannelList* active_channels) const;

private:
    int epoll_fd_;
    std::vector<struct epoll_event> events_;
    std::unordered_map<int, Channel*> channels_;
};

}  // namespace nebula
