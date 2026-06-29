# NebulaIM Network Library

## Overall design

The network library under `common/net` is a lightweight C++17 Reactor implementation for NebulaIM gateway long connections. It uses Linux epoll, nonblocking TCP sockets, eventfd for cross-thread wakeup, timerfd for timers, and the Phase 2 `Buffer` for input/output buffering.

## Reactor model

```text
main EventLoop
  |
  | accept new TCP connections
  v
Acceptor
  |
  | round-robin dispatch
  v
EventLoopThreadPool
  |
  +--> sub EventLoop 1 -> TcpConnection(s)
  +--> sub EventLoop 2 -> TcpConnection(s)
  +--> sub EventLoop N -> TcpConnection(s)
```

A Reactor waits for IO events, dispatches callbacks, and keeps all fd operations for a connection inside its owner EventLoop thread.

## Why epoll

`epoll` scales better than blocking IO and `select` for many long-lived IM connections. It avoids one-thread-per-connection overhead and allows the gateway to manage many sockets with a small number of EventLoop threads.

## Main Reactor and sub Reactors

- Main Reactor owns `Acceptor` and listens for new connections.
- Sub Reactors own accepted `TcpConnection` objects and handle read/write/close/error events.
- `EventLoopThreadPool` assigns new connections to sub Reactors in round-robin order.

## EventLoop

`EventLoop` owns one `EpollPoller`, one wakeup `eventfd`, and one `TimerQueue`. It provides `runInLoop` and `queueInLoop` so cross-thread operations are safely executed in the loop owner thread.

## Channel

`Channel` binds an fd to callbacks and interested events. It does not own the fd. Ownership belongs to `Socket`, `EventLoop`, or `TimerQueue` depending on fd type.

## EpollPoller

`EpollPoller` wraps `epoll_create1`, `epoll_ctl`, and `epoll_wait`. It maps fd to `Channel` and fills active channels for `EventLoop` dispatch. The current implementation uses LT mode.

## TcpConnection lifecycle

1. `Acceptor` accepts a nonblocking fd.
2. `TcpServer` creates `TcpConnection` with shared ownership.
3. The target EventLoop calls `connectEstablished` and enables read events.
4. Read events append data to input `Buffer` and invoke message callback.
5. `send` writes immediately when possible or appends to output `Buffer` and waits for `EPOLLOUT`.
6. Peer close or force close invokes close callback.
7. `TcpServer` removes the connection and calls `connectDestroyed` in the owner loop.

## Buffer and TCP sticky/partial packets

TCP is a byte stream. `Buffer` keeps `reader_index` and `writer_index`, allowing the protocol layer to append bytes, parse complete frames, and leave partial frames unread until more data arrives. This is the foundation for Phase 4 packet decoding.

## TimerQueue and heartbeat

`TimerQueue` uses `timerfd` and supports `runAt`, `runAfter`, and `runEvery`. Gateway heartbeat timeout can later be built by refreshing per-connection timers or periodically scanning last-active timestamps.

## EchoServer

Build and run:

```bash
cmake -S . -B build
cmake --build build -j
./build/examples/echo_server
```

Test with netcat:

```bash
nc 127.0.0.1 9000
hello
```

Expected response:

```text
hello
```

## Interview talking points

1. One thread per connection wastes stack memory, causes excessive context switches, and does not scale for IM long connections.
2. Reactor waits for IO readiness and dispatches callbacks without blocking worker threads on each fd.
3. epoll LT keeps reporting readiness while data remains; ET reports readiness on state changes and requires draining until EAGAIN.
4. Nonblocking sockets prevent one slow fd from blocking the whole EventLoop.
5. Channel does not own fd because fd lifetime differs by type: listener socket, connection socket, eventfd, timerfd.
6. TcpConnection uses `shared_ptr` so callbacks and cross-thread tasks can safely keep the connection alive.
7. `send` may be called from any thread, so it is marshaled into the owner EventLoop to avoid races on fd and output Buffer.
8. Buffer uses reader/writer indexes to avoid frequent erase/copy and to keep partial packet data.
9. Sticky/partial packets are handled by accumulating bytes in Buffer and only consuming complete protocol frames.
10. Multi-Reactor improves concurrency by spreading connection IO across EventLoop threads while keeping each connection single-threaded.

## Common questions

### Why not block in read/write?

Blocking a loop thread would delay all connections assigned to that Reactor.

### Why use eventfd?

It wakes an EventLoop blocked in epoll when another thread queues work.

### Why use timerfd?

It integrates timers into epoll so timer callbacks follow the same event dispatch path as IO.

### How is connection ordering handled?

All operations for one TcpConnection run in one EventLoop thread, preserving per-connection operation order.
