# Backend Completion

## Current Backend Limits

Before phase 13 NebulaIM already had the main TCP/gRPC/Kafka path, but still had production gaps: no native WebSocket access, synchronous Gateway backend RPC, MySQL/Kafka dual-write risk, direct AddFriend without request approval, no conversation table, incomplete delivered/read semantics, no recall, single-device online mapping, incomplete token lifecycle, no rate limiter/circuit breaker, no trace context, no migration workflow, and limited production cleanup/admin surfaces.

## Phase 13 Completion

Phase 13 adds native WebSocket framing and handshake support in the C++ Gateway, a bounded Gateway RPC executor, Outbox Pattern storage, friend request APIs, conversation table/DAO/service, read receipt and recall proto/service support, multi-device connection indexing and Redis online keys, logout/refresh token RPCs, token bucket rate limiting, circuit breaker primitives, trace ID context, migration scripts, admin and conversation services, real backend E2E tests, and final production documentation.

The single-node production hardening pass after phase 13 adds Kafka manual offset commit in PushService, real AdminService online stats and Kafka lag queries, semantic health/readiness scripts, migration locking and pre-migration backup support, Nginx WebSocket security controls, WebSocket push-frame correctness, a browser Web SDK, recall event persistence in the same transaction as the recall update, removal of single-device online compatibility indexes, stale Redis device cleanup, and removal of default test-user seed data.

## Function Matrix

| Area | Status |
|---|---|
| TCP Gateway | Implemented |
| WebSocket Gateway | Implemented, binary frames carry NebulaIM Packet bytes |
| Async Gateway RPC | Implemented with bounded RpcExecutor thread pool |
| User auth | Register/Login/Validate/Logout/RefreshToken |
| Friend relationship | Friend request flow only; direct AddFriend returns `FRIEND_REQUEST_REQUIRED` |
| Message send | MySQL + conversation + outbox transaction path |
| Kafka reliability | OutboxWorker retry/dead status plus PushService manual consumer commit |
| Conversation list | ConversationDao and ConversationService |
| Read receipt | Delivered via ACK, read via MarkMessageRead/MarkConversationRead |
| Recall | Permission and recall-window checks; recall update and outbox event share one transaction |
| Multi-device | Gateway local index and Redis keys are device-scoped only; single-user online indexes are removed |
| Protection | Token bucket and circuit breaker primitives |
| Trace | Thread-local TraceContext, generated TraceId, and gRPC metadata propagation |
| Migration | Versioned SQL files, schema_migrations, MySQL named lock, optional backup, and fail-fast migrate_db.sh |
| Service discovery | Static resolver abstraction, ready for Consul/etcd/K8s replacement |
| Admin security | Scoped SHA-256 admin tokens, metadata auth, audit logs, HealthCheck RPC, bounded cleanup, stale online-device cleanup, online stats, outbox stats, and Kafka lag |
| gRPC TLS/mTLS | Config-driven credentials for service listeners and internal clients |
| Health/readiness | Semantic dependency health checks and systemd ExecStartPre readiness waits |
| Browser SDK | `web_sdk/nebulaim.js` wraps binary WebSocket Packet + protobuf calls |
| E2E | `NEBULA_RUN_BACKEND_E2E=1 ./build/tests/test_backend_final_e2e` covers register/login/friend/message/outbox/push/ack/read/recall/conversation |

## Remaining Production Work

Still not covered: native TLS in the TCP/WebSocket Gateway socket layer, real distributed service discovery cluster backend, Kubernetes Operator, full Jaeger/OpenTelemetry exporter, identity-provider backed admin RBAC, end-to-end encryption, multi-region deployment, full async gRPC completion queue migration, and advanced cleanup governance such as legal hold and per-tenant retention.
