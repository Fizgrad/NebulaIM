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
Gateway ---- GatewayService.DeliverToConnection <---- PushService <---- Kafka/Outbox <---- MessageService
  ^
  | WebSocket binary frame + PacketCodec
Browser
  |                    |                              |                 |
  | gRPC               |                              v                 v
  v                    +--------------------------> Redis            MySQL
UserService / RelationService
  |
  v
MySQL / Redis
```

## Highlights

1. C++17 epoll + Reactor TCP network library.
2. Custom binary protocol with magic/version/body_length/sequence_id.
3. Gateway long-connection access and backend gRPC routing.
4. UserService with MySQL + Redis token auth.
5. MessageService with message_id, dedup, MySQL persistence, conversation update, and Outbox Pattern.
6. PushService with Kafka consume, Redis multi-device online status, Gateway RPC, offline messages, retry, DLQ.
7. MySQL persistence for users, relations, groups, messages, offline messages.
8. Redis for token, online state, dedup, retry.
9. Prometheus/Grafana monitoring skeleton.
10. WebSocket Gateway, async Gateway backend RPC executor, rate limiter, circuit breaker primitives, trace ID context, migration scripts, and benchmark tools.

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
```

## Backend Feature Matrix

| Feature | Status |
|---|---|
| TCP Gateway | Implemented |
| WebSocket Gateway | Implemented, same port auto-detect |
| Gateway async backend RPC | Implemented with RpcExecutor thread pool |
| Outbox Pattern | Implemented for message publication path |
| Friend requests | Implemented in RelationService proto/DAO/service |
| Conversation list | Implemented with ConversationDao and ConversationService |
| Delivered/read semantics | ACK marks delivered; read RPCs mark read |
| Message recall | Implemented with sender check and recall window |
| Multi-device login | Gateway context and Redis online keys support device_id/platform |
| Logout/RefreshToken | Implemented in UserService |
| Rate limiting | TokenBucket/RateLimiter primitives and Gateway wrapper |
| Circuit breaker | Gateway and Push gateway-client paths use circuit breaker primitives |
| Service discovery | Static resolver abstraction for Gateway/Push clients |
| Admin security | Scoped SHA-256 admin tokens, metadata auth, audit logs, and HealthCheck RPC |
| gRPC TLS/mTLS | Config-driven server/client credentials; disabled by default for local dev |
| Trace ID | TraceId/TraceContext plus gRPC metadata propagation; not used as Prometheus labels |
| Database migration | `deploy/mysql/migration/V*.sql` plus `scripts/migrate_db.sh` |

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
```

For integration tests, start dependencies first.

## Documentation

- `docs/backend_completion.md`
- `docs/websocket_gateway.md`
- `docs/outbox_pattern.md`
- `docs/conversation_design.md`
- `docs/reliability_design.md`
- `docs/security_design.md`
- `docs/production_checklist.md`
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

## Current Limits

Still not implemented: native TLS inside the TCP/WebSocket Gateway listener, real distributed service discovery cluster backend, Kubernetes Operator, full Jaeger/OpenTelemetry tracing backend, identity-provider backed admin console, end-to-end encryption, and multi-region deployment. For internet-facing TCP/WebSocket traffic, terminate TLS at a load balancer/Nginx/Envoy or add native TLS to the Gateway socket layer.

## License

TBD
