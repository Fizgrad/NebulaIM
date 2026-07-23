# NebulaIM Benchmark

Build:

```bash
cmake -S . -B build
cmake --build build -j
```

Run examples:

```bash
./build/benchmark/bench_tcp_connections --host 127.0.0.1 --port 9000 --connections 10000 --rate 1000 --duration 60
./build/benchmark/bench_gateway_login --host 127.0.0.1 --port 9000 --requests 10000
./build/benchmark/bench_single_message --host 127.0.0.1 --port 9000 --clients 10 --requests 10000
./build/benchmark/bench_group_message --host 127.0.0.1 --port 9000 --relation-addr 127.0.0.1:50053 --requests 10000
./build/benchmark/bench_push_e2e --host 127.0.0.1 --port 9000 --pairs 10 --requests 1000
```

Reports include QPS, success/failure count, average latency, P50, P90, P99 and max latency.

The business benchmarks use real WebSocket Gateway traffic:

- `bench_gateway_login` creates a new user, then measures Login response latency.
- `bench_single_message` creates sender/receiver users, logs them in, and measures Gateway send response latency.
- `bench_group_message` creates a group through RelationService when `--group-id` is not supplied; when `--group-id` is supplied it first joins the generated sender to that group. Use `--internal-token` if internal RPC auth is enabled.
- `bench_push_e2e` sends a single message and waits until the receiver gets `PUSH_MSG`.

Run `./scripts/health_check.sh config/nebula.conf` first. `bench_push_e2e` also needs PushService and Outbox/Kafka to be healthy.
