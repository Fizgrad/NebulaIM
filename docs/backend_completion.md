# Backend Status

NebulaIM backend is a C++17/Linux distributed IM stack with native TCP and WebSocket Gateway access, gRPC service boundaries, MySQL persistence, Redis online state, Kafka-backed push delivery, scoped AdminService operations, and deployment scripts for single-node production.

## Implemented Capabilities

| Area | Current behavior |
|---|---|
| TCP Gateway | PacketCodec framing over long-lived native TCP connections |
| WebSocket Gateway | Browser WebSocket binary frames carry the same PacketCodec payloads |
| Gateway RPC | Backend calls run through a bounded `RpcExecutor` to keep EventLoop threads non-blocking |
| User auth | Register, login, validate token, logout, refresh token, and user profile lookup |
| Friend relationship | Friend request send/list/accept/reject flow; direct AddFriend requires an approved request |
| Groups | Create, join, leave, member list, and group message delivery |
| Message send | Single and group messages persist through MySQL, conversation updates, and outbox insertion in one transaction |
| Kafka reliability | OutboxWorker retry/dead status and PushService manual offset commit |
| Conversations | Conversation listing, read marker, delete, pin, and mute views |
| Receipts | Delivered state through ACK and read state through message/conversation read APIs |
| Recall | Permission and recall-window checks; recall state and recall outbox event share one transaction |
| Online state | Multi-device Redis keys plus local Gateway connection context |
| Protection | Token bucket rate limiting and circuit breaker primitives |
| Tracing | TraceContext, generated trace/span IDs, gRPC metadata propagation, TraceSpan, TraceManager, and OTLP/HTTP export |
| TLS | Optional native TLS for Gateway TCP/WebSocket listener and configurable gRPC TLS/mTLS |
| Migration | Versioned SQL migrations, schema_migrations table, MySQL named lock, optional backup, and fail-fast migration script |
| Service discovery | Static resolver abstraction for configured service addresses |
| Admin operations | Scoped SHA-256 admin tokens, metadata auth, audit events, health, config validation, service overview, cleanup, online stats, outbox stats, and Kafka lag |
| Health/readiness | Semantic dependency health checks and systemd readiness waits |
| Browser SDK | `web_sdk/nebulaim.js` wraps binary WebSocket Packet + protobuf calls |
| E2E | `NEBULA_RUN_BACKEND_E2E=1 ./build/tests/test_backend_final_e2e` covers register/login/friend/message/outbox/push/ack/read/recall/conversation |

## Current Boundaries

The current backend is production-oriented for a single-node deployment. Cluster service discovery backends, Kubernetes Operator automation, external identity-provider admin RBAC, end-to-end encryption, multi-region routing, native async gRPC completion queues, and advanced retention governance are outside the current implementation.
