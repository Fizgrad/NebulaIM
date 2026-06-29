# Roadmap

## Implemented

- WebSocket support
- Native TLS inside the Gateway TCP/WebSocket listener
- gRPC TLS/mTLS configuration
- Lightweight OTLP/Jaeger tracing exporter
- Gateway RPC executor and business worker pool
- Outbox Pattern
- Static service discovery abstraction
- Rate limiting and circuit breaking primitives
- Multi-device login state
- Read receipts
- Friend request workflow
- Conversation service
- Admin health/cleanup/stats surface
- Admin Kafka lag and Redis-derived online stats
- PushService Kafka manual commit
- Full backend E2E scenario
- Single-node health/readiness scripts and systemd ExecStartPre checks
- Migration locking and pre-migration backup support
- Browser Web SDK for WebSocket Packet/protobuf calls
- Nginx WebSocket Origin/rate/connection hardening
- Admin config validation, service overview, and in-memory audit query

## Remaining

- Real distributed service discovery backend
- Full OpenTelemetry SDK semantics beyond the lightweight exporter
- Large group fanout optimization
- Native gRPC async completion-queue clients
- Search
- Sharding
- CI/CD
- Kubernetes deployment
