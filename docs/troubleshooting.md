# Troubleshooting

- gRPC not found: install `protobuf-compiler-grpc libgrpc++-dev`.
- Protobuf not found: install `protobuf-compiler libprotobuf-dev`.
- MySQL client not found: install `default-libmysqlclient-dev`.
- hiredis not found: install `libhiredis-dev`.
- librdkafka not found: install `librdkafka-dev`.
- MySQL connection failed: check `docker compose ps`, port 3306, user/password.
- Redis failed: check port 6379 and `redis-cli ping`.
- Kafka failed: create topics with `scripts/init_topics.sh`, then check broker metadata with `scripts/health_check.sh`.
- Gateway login failed: ensure UserService is running, Redis is reachable, and the client stays on the same authenticated socket.
- Browser message failed: confirm the frontend sends WebSocket binary frames containing NebulaIM Packet + protobuf bytes, not JSON text frames. Use `web_sdk/nebulaim.js` as the reference.
- Message sent but no push: check `outbox_events` status, Kafka lag through AdminService or Kafka tools, PushService logs, Redis multi-device keys (`nebula:user:devices:*`, `online:{user}:{device}`, `conn:{user}:{device}`), and Gateway logs.
- WebSocket client received unreadable push: verify Gateway is running the version that wraps `GatewayService.DeliverToConnection` output in a WebSocket binary frame for WebSocket connections.
- Prometheus no data: verify metrics ports and scrape targets.
- Grafana no data: verify datasource points to Prometheus.
- P99 high: inspect CPU, DB latency, Kafka lag, outbox backlog, Gateway RPC queue saturation, and circuit breaker state.
- Production readiness: run `./scripts/health_check.sh <config>` and `./scripts/wait_ready.sh <config> gateway`.
- Full E2E: after services are running, run `NEBULA_RUN_BACKEND_E2E=1 ./build/tests/test_backend_final_e2e`.
- Debug: use gdb, valgrind, perf, tcpdump, and Docker logs.
