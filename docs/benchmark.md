# Benchmark Report Template

| Scenario | Connections | Requests | QPS | Avg Latency | P90 | P99 | Success Rate |
|---|---:|---:|---:|---:|---:|---:|---:|
| Login | 100 | 10000 | TBD | TBD | TBD | TBD | TBD |
| Single Message | 100 | 10000 | TBD | TBD | TBD | TBD | TBD |
| Group Message | 100 | 10000 | TBD | TBD | TBD | TBD | TBD |
| Push E2E | 100 pairs | 10000 | TBD | TBD | TBD | TBD | TBD |

## Environment

- CPU:
- Memory:
- OS:
- Compiler:
- MySQL/Redis/Kafka config:

## Bottleneck Analysis

- CPU:
- Memory:
- Network:
- MySQL:
- Redis:
- Kafka:

## Optimization Plan

- Native gRPC async completion queues for the Gateway backend path
- Outbox scan sharding and batching
- Gateway worker pool sizing
- Kafka partition tuning
