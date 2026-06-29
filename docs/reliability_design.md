# Reliability Design

NebulaIM now has basic reliability primitives:

- Rate limiting: token bucket implementation for login, message, and per-connection packet controls.
- Circuit breaker: CLOSED, OPEN, HALF_OPEN states with failure threshold and open timeout.
- Async Gateway RPC: Gateway EventLoop no longer needs to block on synchronous backend stubs when RpcExecutor is enabled.
- Outbox retry: Kafka publication is retried from MySQL outbox state.
- DLQ/dead status: exhausted events are marked dead and can be emitted to the DLQ topic.
- Idempotency: message dedup still uses Redis keying by user and client sequence.
- Trace ID: `TraceContext` carries request trace IDs through logs and payload metadata without using Prometheus labels.
- Service discovery: service clients depend on a resolver abstraction; the current implementation is static config based.
- Admin health and cleanup: AdminService exposes token-protected HealthCheck, bounded RunCleanup, and outbox status stats.
- gRPC TLS: service listeners and internal clients can use TLS/mTLS credentials from config while local development remains plaintext.

Degradation policy: Redis online failures should mark online state unknown/offline; Kafka failures should rely on outbox retry; backend RPC circuit-open should return `SERVICE_UNAVAILABLE`/`CIRCUIT_OPEN` instead of blocking indefinitely.

Outbox workers now claim events before publishing, using a short lease state so multiple workers do not publish the same pending row in normal operation. Consumers still need idempotency because distributed systems can retry after partial failures.

Online debugging starts from trace ID, then checks Gateway logs, service logs, `outbox_events`, Kafka consumer lag, Redis online keys, AdminService HealthCheck/GetOutboxStats, and Prometheus counters. Connection counts still come from Gateway metrics rather than AdminService SQL.
