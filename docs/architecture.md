# NebulaIM Architecture

## Goal

NebulaIM is a C++17/Linux distributed IM system designed to demonstrate production-oriented backend engineering: epoll/Reactor networking, high-concurrency long connections, gRPC service boundaries, middleware integration, observability, tests, benchmarks, and deployable infrastructure.

## High-level topology

```text
Client
  |
  | TCP / WebSocket long connection
  v
nebula-gateway
  |
  | gRPC / Protobuf
  v
+---------------------+
| nebula-user         |
| nebula-message      |
| nebula-relation     |
| nebula-push         |
+---------------------+
  |
  +--> MySQL
  +--> Redis
  +--> Kafka
  +--> Prometheus / Grafana
```

## Service responsibilities

### nebula-gateway

- Accept TCP/WebSocket long connections.
- Decode client packets and validate protocol headers.
- Handle heartbeats, connection close, and graceful shutdown.
- Maintain `user_id -> connection` and `connection_id -> connection` mappings.
- Forward business requests to backend services through gRPC.
- Expose gateway push RPCs for `push_service`.
- Export metrics for connection count, online users, QPS, and latency.

### nebula-user

- Register users.
- Login users.
- Validate password hashes.
- Generate and validate tokens.
- Query user profiles.

### nebula-message

- Process single and group messages.
- Generate message IDs.
- Persist message records.
- Store and pull offline messages.
- Process ACK, deduplication, retry, and ordering rules.
- Produce and consume Kafka messages.

### nebula-relation

- Manage friendships.
- Manage groups and group members.
- Validate whether a user can send group messages.

### nebula-push

- Consume Kafka delivery events.
- Query Redis online status.
- Push to online users through gateway.
- Write offline messages for offline users.
- Send failed deliveries to retry topic and DLQ.

### nebula-common

Shared library modules:

- log
- config
- thread
- net
- db
- redis
- kafka
- monitor
- utils

## Client protocol

The client-to-gateway binary protocol will use a fixed header followed by a Protobuf body:

```cpp
struct PacketHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t type;
    uint32_t sequence_id;
    uint32_t body_length;
};
```

Gateway must validate `magic` and maximum `body_length` before decoding the Protobuf payload.

## Data responsibilities

- MySQL persists users, friendships, groups, group members, messages, and offline messages.
- Redis stores token mappings, online status, connection mappings, rate limits, recent sessions, and deduplication keys.
- Kafka decouples message send, group fanout, offline delivery, retry, and DLQ paths.

## Phase 1 scope

This phase only creates a buildable skeleton:

- repository directories
- top-level CMake
- service CMake files
- minimal service `main.cpp` files
- initial README and architecture docs
- Docker Compose dependencies
- MySQL initialization SQL
- Prometheus configuration

No business logic, network library, protocol codec, gRPC codegen, or storage client implementation is included in Phase 1.
