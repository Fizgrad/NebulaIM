#pragma once

#include <cstdint>
#include <functional>

namespace nebula {

class EventLoop;

class Channel {
public:
    using EventCallback = std::function<void()>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;

    void handleEvent();

    void setReadCallback(EventCallback cb);
    void setWriteCallback(EventCallback cb);
    void setCloseCallback(EventCallback cb);
    void setErrorCallback(EventCallback cb);

    int fd() const;
    uint32_t events() const;
    void setRevents(uint32_t revents);

    bool isNoneEvent() const;
    bool isWriting() const;

    void enableReading();
    void enableWriting();
    void disableWriting();
    void disableAll();
    void remove();

    int index() const;
    void setIndex(int index);

    EventLoop* ownerLoop();

private:
    void update();

private:
    EventLoop* loop_;
    const int fd_;
    uint32_t events_;
    uint32_t revents_;
    int index_;

    EventCallback read_callback_;
    EventCallback write_callback_;
    EventCallback close_callback_;
    EventCallback error_callback_;
};

}  // namespace nebula
