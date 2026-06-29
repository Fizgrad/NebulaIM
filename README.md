<p align="center">
  <img src="docs/assets/logo.png" alt="NebulaIM Logo" width="180">
</p>

# NebulaIM

NebulaIM is a C++17 high-performance distributed instant messaging system featuring a custom epoll/Reactor TCP gateway, binary packet protocol, gRPC microservices, MySQL, Redis, Kafka, Prometheus/Grafana, and benchmark tooling.

## Architecture

```text
Client
  | TCP + PacketCodec
  v
Gateway ---- GatewayService.DeliverToConnection <---- PushService <---- Kafka <---- MessageService
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
5. MessageService with message_id, dedup, MySQL persistence, Kafka produce.
6. PushService with Kafka consume, Redis online status, Gateway RPC, offline messages, retry, DLQ.
7. MySQL persistence for users, relations, groups, messages, offline messages.
8. Redis for token, online state, dedup, retry.
9. Prometheus/Grafana monitoring skeleton.
10. Benchmark tools for connection/login/message/push scenarios.

## Quick Start

```bash
./scripts/build.sh
./scripts/start_deps.sh
./scripts/init_topics.sh
./scripts/start_services.sh
```

Run a client:

```bash
./build/examples/gateway_client_demo --host 127.0.0.1 --port 9000
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

No WebSocket/TLS yet, Gateway backend calls are synchronous, Outbox Pattern is documented but not implemented, service discovery/config center/tracing are future work.

## License

TBD
