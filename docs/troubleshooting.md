# Troubleshooting

- gRPC not found: install `protobuf-compiler-grpc libgrpc++-dev`.
- Protobuf not found: install `protobuf-compiler libprotobuf-dev`.
- MySQL client not found: install `default-libmysqlclient-dev`.
- hiredis not found: install `libhiredis-dev`.
- librdkafka not found: install `librdkafka-dev`.
- MySQL connection failed: check `docker compose ps`, port 3306, user/password.
- Redis failed: check port 6379 and `redis-cli ping`.
- Kafka failed: create topics with `scripts/init_topics.sh`.
- Gateway login failed: ensure UserService is running.
- Message sent but no push: check Kafka topic, PushService logs, Redis online keys.
- Prometheus no data: verify metrics ports and scrape targets.
- Grafana no data: verify datasource points to Prometheus.
- P99 high: inspect CPU, DB latency, Kafka lag, blocking gRPC in Gateway.
- Debug: use gdb, valgrind, perf, tcpdump, and Docker logs.
