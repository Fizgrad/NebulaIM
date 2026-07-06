<p align="center">
  <img src="docs/assets/logo.png" alt="NebulaIM Logo" width="180">
</p>

# NebulaIM

NebulaIM is a C++17 high-performance distributed instant messaging system featuring a custom epoll/Reactor TCP/WebSocket gateway, binary packet protocol, gRPC microservices, MySQL, Redis, Kafka, Prometheus/Grafana, and benchmark tooling.

## Architecture

```text
Native Client
  | TCP + PacketCodec
  v
Gateway -- bounded gRPC/RpcExecutor --> UserService / RelationService / MessageService
  ^
  | WebSocket binary frame + PacketCodec
Browser

MessageService --> MySQL(messages + conversations + outbox_events)
                         |
                         v
                    OutboxWorker --> Kafka --> PushService -- GatewayService.DeliverToConnection --> Gateway

UserService / RelationService / ConversationService --> MySQL / Redis
AdminService --> health / cleanup / online stats / outbox stats / Kafka lag / config validation / audit
```

## Highlights

1. C++17 epoll + Reactor TCP network library.
2. Custom binary protocol with magic/version/body_length/sequence_id.
3. Gateway long-connection access and backend gRPC routing.
4. UserService with MySQL + Redis token auth.
5. MessageService with message_id, dedup, MySQL persistence, conversation update, and Outbox Pattern.
6. PushService with Kafka manual commit after successful handling, multi-consumer worker support, Redis multi-device online status, Gateway RPC, offline messages, retry, DLQ.
7. MySQL persistence for users, relations, groups, messages, offline messages.
8. Redis for token, online state, dedup, retry.
9. Prometheus/Grafana monitoring assets.
10. WebSocket Gateway, optional native Gateway TLS, bounded async Gateway backend RPC executor, rate limiter, circuit breaker primitives, trace ID + OTLP/Jaeger tracing, migration scripts, health/readiness scripts, Web SDK, and benchmark tools.

## Quick Start

```bash
./scripts/build.sh
./scripts/start_deps.sh
./scripts/migrate_db.sh
./scripts/init_topics.sh
./scripts/start_services.sh
```

Single-node production deployment:

```bash
cmake -S . -B build
cmake --build build -j
./scripts/render_admin_token_hash.sh 'replace-with-long-random-token'
./scripts/validate_prod_config.sh /etc/nebulaim/nebula.conf
sudo ./scripts/install_single_node.sh
```

See `docs/single_node_production.md` for systemd, Nginx TLS termination, backups, and health checks.

Run a client:

```bash
./build/examples/gateway_client_demo --host 127.0.0.1 --port 9000
./build/examples/gateway_websocket_client_demo --url ws://127.0.0.1:9000/
```

Run the real backend E2E scenario after services are running:

```bash
NEBULA_RUN_BACKEND_E2E=1 ./build/tests/test_backend_final_e2e
```

Stop:

```bash
./scripts/stop_services.sh
./scripts/stop_deps.sh
```

## Dependencies

Ubuntu/Debian:

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config libssl-dev protobuf-compiler libprotobuf-dev protobuf-compiler-grpc libgrpc++-dev default-libmysqlclient-dev libhiredis-dev librdkafka-dev
```

## Build

```bash
mkdir -p build
cd build
cmake ..
cmake --build . -j
```

## Services and Ports

| Service | Port |
|---|---:|
| Gateway TCP | 9000 |
| Gateway RPC | 50055 |
| UserService | 50051 |
| MessageService | 50052 |
| RelationService | 50053 |
| PushService | 50054 |
| ConversationService | 50056 |
| AdminService | 50057 |
| Prometheus | 9090 |
| Grafana | 3000 |
| Jaeger UI | 16686 |
| OTLP HTTP | 4318 |
| MySQL | 3306 |
| Redis | 6379 |
| Kafka | 9092 |

## Monitoring

Prometheus: http://localhost:9090
Grafana: http://localhost:3000, default admin password configured in `deploy/docker-compose.yml`.

Metrics endpoints:

```text
Gateway 9100
UserService 9101
MessageService 9102
RelationService 9103
PushService 9104
ConversationService 9105
AdminService 9106
```

## Backend Feature Matrix

| Feature | Status |
|---|---|
| TCP Gateway | Implemented |
| WebSocket Gateway | Implemented, same port auto-detect |
| Gateway async backend RPC | Implemented with bounded RpcExecutor thread pool |
| Outbox Pattern | Implemented for message send and recall publication paths |
| Kafka consumer safety | PushService disables auto commit and commits offsets after successful handling |
| Kafka producer safety | KafkaProducer waits for delivery callback acknowledgement |
| Friend requests | Implemented in RelationService proto/DAO/service; direct AddFriend is rejected in production path |
| Conversation list | Implemented with ConversationDao and ConversationService |
| Delivered/read semantics | ACK marks delivered; read RPCs mark read |
| Message recall | Implemented with sender check and recall window |
| Multi-device login | Gateway context, local index, and Redis online keys are device-scoped only |
| Logout/RefreshToken | Implemented in UserService |
| Rate limiting | TokenBucket/RateLimiter primitives and Gateway wrapper |
| Circuit breaker | Gateway and Push gateway-client paths use circuit breaker primitives |
| Service discovery | Static resolver abstraction for Gateway/Push clients |
| Admin security | Scoped SHA-256 admin tokens, metadata auth, audit logs, HealthCheck, real online stats, outbox stats, and Kafka lag |
| Internal RPC security | Optional `x-nebula-internal-token` metadata auth across Gateway/User/Relation/Conversation/Message/Push RPC surfaces |
| gRPC TLS/mTLS | Config-driven server/client credentials; disabled by default for local dev |
| Gateway native TLS | Optional TLS for the TCP/WebSocket Gateway listener through `gateway.tls.*`; edge TLS via Nginx remains supported |
| Tracing | TraceId/TraceContext, span wrapper, lightweight OTLP/HTTP exporter, Jaeger Compose service; trace IDs are not Prometheus labels |
| Database migration | `deploy/mysql/migration/V*.sql` plus locked/backup-aware `scripts/migrate_db.sh` |
| Production readiness | `health_check.sh`, `wait_ready.sh`, systemd ExecStartPre checks, and Nginx WebSocket hardening |
| Browser SDK | `web_sdk/nebulaim.js` wraps WebSocket binary Packet + protobuf calls |

## Benchmark

```bash
./build/benchmark/bench_tcp_connections --host 127.0.0.1 --port 9000 --connections 10000 --rate 1000 --duration 60
./build/benchmark/bench_gateway_login --host 127.0.0.1 --port 9000 --clients 100 --requests 10000
./build/benchmark/bench_single_message --host 127.0.0.1 --port 9000 --clients 100 --requests 10000
./build/benchmark/bench_group_message --host 127.0.0.1 --port 9000 --clients 100 --groups 10 --members 100 --requests 10000
./build/benchmark/bench_push_e2e --host 127.0.0.1 --port 9000 --pairs 100 --requests 10000
```

## Tests

```bash
./scripts/run_tests.sh --unit-only
ctest --test-dir build -L unit --output-on-failure
```

For integration tests, start dependencies first, then run:

```bash
ctest --test-dir build -L integration --output-on-failure
```

Full backend E2E is opt-in so normal `ctest` remains usable without running services:

```bash
NEBULA_RUN_BACKEND_E2E=1 ./build/tests/test_backend_final_e2e
```

## Documentation

- `docs/backend_completion.md`
- `docs/websocket_gateway.md`
- `docs/outbox_pattern.md`
- `docs/conversation_design.md`
- `docs/reliability_design.md`
- `docs/security_design.md`
- `docs/production_checklist.md`
- `docs/tracing.md`
- `docs/admin_operations.md`
- `docs/single_node_production.md`
- `docs/final_architecture.md`
- `docs/deployment.md`
- `docs/storage.md`
- `docs/gateway.md`
- `docs/message_service.md`
- `docs/push_service.md`
- `docs/benchmark.md`
- `docs/interview.md`
- `docs/troubleshooting.md`
- `docs/roadmap.md`
- `web_sdk/README.md`

## Current Limits

Still not implemented: real distributed service discovery cluster backend, Kubernetes Operator, full OpenTelemetry SDK features such as baggage/context propagation beyond trace IDs, identity-provider backed admin console, end-to-end encryption, multi-region deployment, and native gRPC completion-queue clients. For internet-facing TCP/WebSocket traffic, either terminate TLS at Nginx/Envoy or enable native Gateway TLS with `gateway.tls.enabled=true`.

## License

No standalone license file is currently included in this repository.
