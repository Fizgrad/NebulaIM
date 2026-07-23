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
| ConversationService RPC | 50056 |
| AdminService RPC | 50057 |
| DeviceService RPC | 50058 |
| Gateway Metrics | 9100 |
| UserService Metrics | 9101 |
| MessageService Metrics | 9102 |
| RelationService Metrics | 9103 |
| PushService Metrics | 9104 |
| ConversationService Metrics | 9105 |
| AdminService Metrics | 9106 |
| DeviceService Metrics | 9107 |
| Prometheus | 9090 |
| Grafana | 3000 |
| Jaeger UI | 16686 |
| OTLP HTTP | 4318 |
| MySQL | 3306 |
| Redis | 6379 |
| Kafka | 9092 |

## Commands

```bash
./scripts/build.sh
./scripts/start_deps.sh
./scripts/migrate_db.sh
./scripts/init_topics.sh
./scripts/start_services.sh
./scripts/health_check.sh config/nebula.conf
```

Local `start_services.sh` waits for each service dependency through `wait_ready.sh`, launches services with `setsid nohup`, writes logs to `logs/`, writes pids to `run/`, and fails fast if a process exits immediately after start. When `NEBULA_ENV=prod`, it refuses to use `config/nebula.conf` and runs `validate_prod_config.sh` before starting.

For single-node production with systemd/Nginx/backups, use `docs/single_node_production.md` instead of the local `start_services.sh` workflow.

AdminService is protected by scoped `admin_service.admin_tokens`. The tracked development config leaves admin tokens empty, so AdminService denies admin RPCs until a local or production config provides token hashes.

```text
admin_service.admin_tokens=ops:sha256:<token_sha256_hex>:health,stats,outbox,kafka,cleanup
```

Admin clients must send the raw token through gRPC metadata key `x-nebula-admin-token`. Each AdminService RPC emits an audit log line with action, required scope, principal, and request ID.

Internal service-to-service gRPC can be protected with metadata auth:

```text
internal_rpc.auth.enabled=true
internal_rpc.auth.token=<raw-internal-token>
internal_rpc.auth.token_sha256=<sha256-of-raw-internal-token>
```

Gateway and PushService inject `x-nebula-internal-token` for their internal calls. Business services and GatewayService reject missing or invalid internal tokens when the switch is enabled.

Admin cleanup is bounded by config:

```text
admin.cleanup.outbox_published_retention_ms=604800000
admin.cleanup.offline_acked_retention_ms=604800000
admin.cleanup.friend_request_retention_ms=2592000000
admin.cleanup.message_receipt_retention_ms=7776000000
admin.cleanup.batch_size=1000
```

Use `RunCleanup(dry_run=true)` before destructive cleanup. Cleanup removes expired MySQL rows and prunes stale Redis online device members whose per-device online/connection keys have expired. AdminService also exposes real Redis-derived online user/connection counts, Kafka lag, production config validation, service overview, and recent audit events through `GetSystemStats`, `GetKafkaLagInfo`, `ValidateConfig`, `GetServiceOverview`, and `ListAuditEvents`.

## gRPC TLS

Local development defaults to plaintext:

```text
grpc.tls.enabled=false
```

For production service-to-service encryption, place readable PEM files on every service host and set:

```text
grpc.tls.enabled=true
grpc.tls.ca_cert_path=/etc/nebula/certs/ca.pem
grpc.tls.server_cert_path=/etc/nebula/certs/server.pem
grpc.tls.server_key_path=/etc/nebula/certs/server-key.pem
grpc.tls.client_cert_path=/etc/nebula/certs/client.pem
grpc.tls.client_key_path=/etc/nebula/certs/client-key.pem
grpc.tls.require_client_auth=true
grpc.tls.target_name_override=nebula.internal
```

All C++ gRPC service listeners and internal Gateway/Push clients read these keys.

Internal RPC token auth and gRPC TLS are complementary. For single-node production, keep internal RPC listeners on `127.0.0.1`; enable the token guard and use TLS/mTLS if ports leave loopback or cross hosts.

## Gateway Native TLS

Public TCP/WebSocket TLS can still be terminated at Nginx/Envoy. If the C++ Gateway must terminate TLS directly, configure:

```text
gateway.tls.enabled=true
gateway.tls.cert_path=/etc/nebulaim/tls/gateway.crt
gateway.tls.key_path=/etc/nebulaim/tls/gateway.key
gateway.tls.ca_cert_path=
gateway.tls.require_client_auth=false
```

`scripts/health_check.sh` uses `openssl s_client` for the Gateway listener when `gateway.tls.enabled=true`. Use `ws://` when disabled and `wss://` when enabled.

## Tracing

Docker Compose includes Jaeger all-in-one with OTLP/HTTP on `127.0.0.1:4318` and UI on `127.0.0.1:16686` in production override mode. Enable tracing with:

```text
trace.enabled=true
trace.service_name=nebulaim
trace.otlp_endpoint=http://127.0.0.1:4318/v1/traces
trace.export_timeout_ms=2000
trace.batch_size=64
trace.flush_interval_ms=1000
trace.max_queue_size=4096
```

The lightweight exporter sends OTLP/HTTP JSON batches. It is intentionally dependency-light and does not implement the full OpenTelemetry C++ SDK surface.

## Database Migration

Migration files live in `deploy/mysql/migration/` and are applied in lexical version order. Applied versions are recorded in `schema_migrations`.

```bash
./scripts/migrate_db.sh config/nebula.conf
```

The script reads MySQL connection settings from the config path argument or `NEBULA_CONFIG`; explicit `MYSQL_HOST`, `MYSQL_PORT`, `MYSQL_USER`, `MYSQL_PASSWORD`, and `MYSQL_DATABASE` environment variables still override config values. It acquires a MySQL named lock before applying migrations, exits on the first failed migration, and skips versions already present in `schema_migrations`. In production mode (`NEBULA_ENV=production`) or when `NEBULA_MIGRATE_BACKUP=true`, it runs `scripts/backup_mysql.sh` before applying new versions. Rollback is an operational restore using `scripts/restore_mysql.sh` or hand-written rollback SQL.

## Readiness And Health

`scripts/wait_ready.sh` waits for the dependencies required by a service. The systemd units call it from `ExecStartPre` after config validation, so service order is not treated as readiness.

```bash
./scripts/wait_ready.sh /etc/nebulaim/nebula.conf gateway
```

`scripts/health_check.sh` checks MySQL query, Redis ping, Kafka metadata, outbox pending/dead status, service ports, and systemd activity when systemd is available.

`scripts/stop_services.sh` sends SIGTERM to all pid-file services, waits up to `NEBULA_STOP_TIMEOUT_SECONDS` seconds, and uses SIGKILL only for processes that remain alive. NebulaIM service binaries handle SIGTERM/SIGINT by shutting down gRPC servers; PushService then closes its Kafka consumers before exit.

## Manual Service Startup

```bash
./build/user_service/nebula_user_service --config config/nebula.conf
./build/relation_service/nebula_relation_service --config config/nebula.conf
./build/conversation_service/nebula_conversation_service --config config/nebula.conf
./build/message_service/nebula_message_service --config config/nebula.conf
./build/push_service/nebula_push_service --config config/nebula.conf
./build/gateway/nebula_gateway --config config/nebula.conf
./build/device_service/nebula_device_service --config config/nebula.conf
./build/admin_service/nebula_admin_service --config config/nebula.conf
```
