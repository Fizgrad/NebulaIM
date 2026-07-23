# Benchmark

NebulaIM benchmark binaries generate real Gateway traffic. They report total requests, success/failure count, duration, QPS, average latency, P50, P90, P99 and max latency.

## Scenarios

| Tool | Path measured |
|---|---|
| `bench_tcp_connections` | Raw TCP connect and hold |
| `bench_gateway_login` | WebSocket handshake, Register setup, Login response latency |
| `bench_single_message` | WebSocket Login setup, SendSingleMessage response latency |
| `bench_group_message` | WebSocket Login setup, optional RelationService CreateGroup, SendGroupMessage response latency |
| `bench_push_e2e` | WebSocket Login setup, SendSingleMessage to receiver `PUSH_MSG` latency |

## Run

```bash
./scripts/health_check.sh config/nebula.conf
./build/benchmark/bench_tcp_connections --host 127.0.0.1 --port 9000 --connections 1000 --rate 100 --duration 30
./build/benchmark/bench_gateway_login --host 127.0.0.1 --port 9000 --requests 1000
./build/benchmark/bench_single_message --host 127.0.0.1 --port 9000 --clients 10 --requests 1000
./build/benchmark/bench_group_message --host 127.0.0.1 --port 9000 --relation-addr 127.0.0.1:50053 --requests 1000
./build/benchmark/bench_push_e2e --host 127.0.0.1 --port 9000 --pairs 10 --requests 100
```

If internal RPC auth is enabled, pass the raw internal token to the group benchmark so it can create the setup group:

```bash
./build/benchmark/bench_group_message --internal-token "$NEBULA_INTERNAL_TOKEN"
```

To benchmark an existing group instead, pass `--group-id`; the benchmark first joins the generated sender user to that group through RelationService.

## Environment

- CPU: record target server CPU model and core count before publishing numbers.
- Memory: record total memory and service/container limits.
- OS: record kernel and distribution version.
- Compiler: record compiler and CMake build type.
- MySQL/Redis/Kafka config: record pool sizes, Kafka partitions, Redis persistence mode, and retention settings.

## Bottleneck Analysis

- CPU: inspect Gateway EventLoop threads, RpcExecutor threads, and service worker saturation.
- Memory: inspect Gateway connection buffers, bounded RPC queue size, and Kafka consumer backlog.
- Network: separate public WebSocket/TLS overhead from internal gRPC/Kafka traffic.
- MySQL: inspect slow queries, connection pool waits, outbox backlog, and conversation update cost.
- Redis: inspect online-state key TTL refresh latency and command errors.
- Kafka: inspect broker health, topic partitioning, consumer group lag, retry, and DLQ growth.

## Optimization Plan

- Native gRPC async completion queues for the Gateway backend path
- Outbox scan sharding and batching
- Gateway worker pool sizing
- Kafka partition tuning

## Required validation before using results

Run these checks on the same build and config used for the benchmark:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
./scripts/health_check.sh config/nebula.conf
NEBULA_RUN_BACKEND_E2E=1 ./build/tests/test_backend_final_e2e
```

Benchmark figures should not be treated as production capacity until the semantic health check and full backend E2E pass on the target host.
