# NebulaIM Benchmark

Build:

```bash
cmake -S . -B build
cmake --build build -j
```

Run examples:

```bash
./build/benchmark/bench_tcp_connections --host 127.0.0.1 --port 9000 --connections 10000 --rate 1000 --duration 60
./build/benchmark/bench_gateway_login --host 127.0.0.1 --port 9000 --clients 100 --requests 10000
./build/benchmark/bench_single_message --host 127.0.0.1 --port 9000 --clients 100 --requests 10000
./build/benchmark/bench_group_message --host 127.0.0.1 --port 9000 --clients 100 --groups 10 --members 100 --requests 10000
./build/benchmark/bench_push_e2e --host 127.0.0.1 --port 9000 --pairs 100 --requests 10000
```

Reports include QPS, success/failure count, average latency, P50, P90, P99 and max latency.
