# Benchmark Report

| Scenario | Connections | Requests | QPS | Avg Latency | P90 | P99 | Success Rate |
|---|---:|---:|---:|---:|---:|---:|---:|
| Login | 100 | 10000 | Measure per environment | Measure per environment | Measure per environment | Measure per environment | Measure per environment |
| Single Message | 100 | 10000 | Measure per environment | Measure per environment | Measure per environment | Measure per environment | Measure per environment |
| Group Message | 100 | 10000 | Measure per environment | Measure per environment | Measure per environment | Measure per environment | Measure per environment |
| Push E2E | 100 pairs | 10000 | Measure per environment | Measure per environment | Measure per environment | Measure per environment | Measure per environment |

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
