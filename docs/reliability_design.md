# Reliability Design

NebulaIM now has basic reliability primitives:

- Rate limiting: token bucket implementation for login, message, and per-connection packet controls.
- Circuit breaker: CLOSED, OPEN, HALF_OPEN states with failure threshold and open timeout.
- Async Gateway RPC: Gateway EventLoop no longer needs to block on synchronous backend stubs when RpcExecutor is enabled; the executor has a configurable max queue and returns `SERVICE_UNAVAILABLE` when saturated.
- Outbox retry: Kafka publication is retried from MySQL outbox state.
- DLQ/dead status: exhausted events are marked dead and can be emitted to the DLQ topic.
- Kafka consumer acknowledgement: PushService disables auto commit and commits offsets only after online delivery, retry enqueue, DLQ/offline persistence, or final handled outcome succeeds.
- Idempotency: message dedup still uses Redis keying by user and client sequence.
- Trace ID and spans: `TraceContext` carries request trace IDs through logs and payload metadata without using Prometheus labels. `TraceSpan` records Gateway, MessageService, and PushService spans and `TraceManager` exports batches to OTLP/HTTP for Jaeger.
- Service discovery: service clients depend on a resolver abstraction; the current implementation is static config based.
- Redis client safety: `RedisClient` protects a single hiredis connection with a mutex so multi-Reactor Gateway callbacks cannot interleave commands on the same connection.
- Admin health and cleanup: AdminService exposes token-protected HealthCheck, bounded RunCleanup, stale Redis online-device cleanup, online/connection stats, outbox status stats, Kafka lag queries, config validation, service overview, and recent audit events.
- gRPC TLS: service listeners and internal clients can use TLS/mTLS credentials from config while local development remains plaintext.
- Gateway native TLS: the TCP/WebSocket listener can terminate TLS directly through `gateway.tls.*`, while Nginx/Envoy edge termination remains supported.
- Readiness: `wait_ready.sh` and systemd `ExecStartPre` wait for semantic dependency readiness before starting dependent services.

Degradation policy: Redis online failures should mark online state unknown/offline; Kafka failures should rely on outbox retry; backend RPC circuit-open should return `SERVICE_UNAVAILABLE`/`CIRCUIT_OPEN` instead of blocking indefinitely.

Outbox workers now claim events before publishing, using a short lease state so multiple workers do not publish the same pending row in normal operation. Message send and message recall both write the domain update and outbox event in the same local MySQL transaction. Consumers still need idempotency because distributed systems can retry after partial failures.

Online debugging starts from trace ID or Jaeger trace, then checks Gateway logs for login bind/online-write results, PushService logs for Kafka poll/commit and delivery decisions, `outbox_events`, Kafka consumer lag, Redis multi-device online keys, stale device members, AdminService HealthCheck/GetSystemStats/GetOutboxStats/GetKafkaLagInfo/GetServiceOverview/ListAuditEvents, and Prometheus counters.
