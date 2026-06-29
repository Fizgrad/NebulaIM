# Deployment

## Ports

| Service | Port |
|---|---|
| Gateway TCP | 9000 |
| Gateway RPC | 50055 |
| UserService RPC | 50051 |
| MessageService RPC | 50052 |
| RelationService RPC | 50053 |
| PushService RPC | 50054 |
| Gateway Metrics | 9100 |
| UserService Metrics | 9101 |
| MessageService Metrics | 9102 |
| RelationService Metrics | 9103 |
| PushService Metrics | 9104 |
| Prometheus | 9090 |
| Grafana | 3000 |
| MySQL | 3306 |
| Redis | 6379 |
| Kafka | 9092 |

## Commands

```bash
./scripts/build.sh
./scripts/start_deps.sh
./scripts/init_topics.sh
./scripts/start_services.sh
```

Logs are in `logs/`; pids are in `run/`.
