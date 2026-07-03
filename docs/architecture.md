# NebulaIM Architecture

## Goal

NebulaIM is a C++17/Linux distributed IM system with epoll/Reactor networking, high-concurrency TCP/WebSocket long connections, gRPC service boundaries, MySQL/Redis/Kafka integration, observability, tests, benchmarks, and deployable single-node infrastructure.

## High-level topology

```text
Native Client -> TCP PacketCodec --------------------+
                                                     v
Browser -> WebSocket binary frame -> PacketCodec -> Gateway
                                                     |
                                                     | bounded gRPC/RpcExecutor
                                                     v
       +-------------+  +----------------+  +----------------------+  +-------------+
       | UserService |  | RelationService|  | ConversationService  |  | MessageSvc  |
       +-------------+  +----------------+  +----------------------+  +-------------+
              |                 |                    |                       |
              +-----------------+--------------------+-----------------------+
                                      MySQL / Redis
                                                                  |
                                                                  | outbox_events
                                                                  v
                                                            OutboxWorker -> Kafka
                                                                               |
                                                                               v
                                                                      PushService
                                                                               |
                                                                               | GatewayService.DeliverToConnection
                                                                               v
                                                                            Gateway
```

AdminService provides token-protected health, cleanup, online stats, outbox stats, Kafka lag, config validation, service overview, and audit-event query for operations. Prometheus/Grafana provide metrics dashboards, and Jaeger receives OTLP/HTTP trace spans when tracing is enabled.

## Service responsibilities

### Gateway

- Accept native TCP Packet clients and browser WebSocket clients on the same port.
- Optionally terminate native TLS for TCP/WebSocket clients at the Gateway socket layer.
- Decode PacketCodec frames and validate protocol headers.
- Handle register/login/heartbeat/message/ACK/offline-pull packets.
- Dispatch blocking backend gRPC stubs through a bounded `RpcExecutor`.
- Maintain local connection context, including `device_id`, `platform`, and WebSocket transport flag.
- Write Redis online state using multi-device keys only.
- Expose `GatewayService` for PushService and wrap push packets correctly for TCP or WebSocket connections.

### UserService

- Register users.
- Login users with password hashing.
- Generate, validate, refresh, and delete tokens.
- Persist/update device metadata.
- Query user profiles.

### RelationService

- Manage friendships.
- Manage friend request send/accept/reject/list flow.
- Manage groups and group members.

### ConversationService

- List conversations.
- Mark conversations read.
- Delete/pin/mute conversation views.

### MessageService

- Process single and group messages.
- Generate message IDs and conversation IDs.
- Deduplicate client retries.
- Persist messages and update conversations.
- Insert outbox events in the same transaction as message send.
- Mark delivered/read states.
- Recall messages with permission and time-window checks, committing recall state and recall event together.

### PushService

- Consume Kafka delivery events with auto commit disabled.
- Check Redis multi-device online state.
- Push online messages through GatewayService.
- Write offline messages for offline users.
- Send failed deliveries to retry topic and DLQ.
- Commit Kafka offsets only after handling succeeds.

### AdminService

- Authenticate scoped SHA-256 admin tokens through gRPC metadata.
- Run bounded cleanup.
- Report dependency health, online stats, outbox stats, and Kafka lag.
- Validate production-risk config, report service overview, and expose recent audit events.

## Data responsibilities

- MySQL persists users, devices, friend requests, friendships, groups, group members, messages, conversations, receipts, offline messages, outbox events, and schema migration state.
- Redis stores tokens, multi-device online state, rate limits, recent sessions, retry counters, and deduplication keys with TTL.
- Kafka decouples message delivery, group fanout, retry, and DLQ paths.

## Reliability boundaries

- Gateway does not write MySQL and does not consume Kafka.
- MessageService does not push directly to clients.
- Outbox makes MySQL the authoritative local transaction and Kafka publication eventually consistent.
- PushService manual commit reduces consumer-side message loss windows.
- `health_check.sh` and `wait_ready.sh` provide single-node semantic readiness checks.
- `TraceSpan`/`TraceManager` export lightweight OTLP/HTTP traces to Jaeger without using trace IDs as Prometheus labels.
