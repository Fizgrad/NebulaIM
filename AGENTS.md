# AGENTS.md

## Repository state
- This repository is intentionally empty at creation time; do not infer missing conventions from absent files.
- The first implementation task must start with Phase 1 project skeleton, not full business logic.

## Product target
- Build NebulaIM: a C++17/Linux high-performance distributed IM system, not a toy demo.
- Required stack: CMake, epoll/Reactor, TCP/WebSocket gateway, Protobuf/gRPC, MySQL, Redis, Kafka, Prometheus/Grafana, Docker Compose, tests, benchmark tools, docs.
- Target services: `gateway`, `user_service`, `message_service`, `relation_service`, `push_service`, shared `common/`, and `proto/`.

## Required implementation order
1. Skeleton: directories, top-level CMake, initial README/docs, Docker Compose, SQL, Prometheus config, minimal service mains.
2. Common library: log, config, thread pool, Buffer, time utilities, error codes, unit tests.
3. Network library under `common/net`: `EventLoop`, `Channel`, `EpollPoller`, `TcpServer`, `TcpConnection`, `Acceptor`, `Socket`, `InetAddress`, `Buffer`, `EventLoopThread`, `EventLoopThreadPool`, `Timer`, `TimerQueue`.
4. Protocol layer: `PacketHeader`, codec, Protobuf body handling, sticky/partial packet handling, validation tests.
5. Protobuf/gRPC files and CMake code generation.
6. Storage access: MySQL pool, Redis wrapper, Kafka producer/consumer, CRUD tests, init SQL.
7. Implement services in order: user, relation, message, push, gateway.
8. Monitoring/benchmark, then final docs/interview material.

## Project layout to preserve
- Use these top-level paths: `common/`, `gateway/`, `user_service/`, `message_service/`, `relation_service/`, `push_service/`, `proto/`, `tests/`, `benchmark/`, `scripts/`, `deploy/`, `docs/`, `third_party/`.
- Each service must have its own `CMakeLists.txt`, `include/`, `src/`, and a small `main.cpp` entrypoint.
- Deployment assets belong in `deploy/`; docs belong in `docs/`; generated/build output belongs outside source, e.g. `build/`.

## C++ conventions
- Use C++17, RAII, smart pointers, move semantics, atomics/condition variables where appropriate.
- Class names use PascalCase; private members use trailing underscores (`fd_`, `loop_`).
- Keep networking and business logic decoupled; do not put service logic in `main.cpp`.
- Avoid ownership-ambiguous raw pointers and naked `new`/`delete`.
- Avoid magic numbers in business code; centralize protocol constants, limits, and error codes.
- Add comments only for important classes or complex logic; do not add obvious comments.
- All services must support config files and graceful shutdown.

## Network/protocol requirements
- Implement nonblocking sockets with epoll LT mode first; keep ET mode extensible.
- Main Reactor accepts connections; sub Reactors handle connection IO.
- Support connection/message/close callbacks, write buffers, timers, heartbeat timeout, and graceful close.
- Client protocol header is `magic`, `version`, `type`, `sequence_id`, `body_length`; body is Protobuf.
- Always validate magic and max body length before decoding to prevent oversized packets.

## Data/middleware design requirements
- MySQL schema must include users, friendships, groups, group_members, messages, offline_messages with indexes for `user_id`, `conversation_id`, `message_id`, and `created_at` query paths.
- Redis keys must include token, online status, connection mapping, rate limit, recent sessions, and message dedup keys with explicit TTL/failure handling.
- Kafka topics must include single, group, offline, retry, and DLQ topics; choose keys to preserve per-conversation ordering where possible.

## Verification
- After CMake exists, prefer: `cmake -S . -B build`, `cmake --build build`, then `ctest --test-dir build --output-on-failure`.
- Do not claim tests/lint/typecheck pass unless the commands were run in this workspace.
- If adding third-party dependencies, document installation and keep the skeleton buildable step by step.
