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
./scripts/migrate_db.sh
./scripts/init_topics.sh
./scripts/start_services.sh
./scripts/health_check.sh config/nebula.conf
```

Local `start_services.sh` launches services with `setsid nohup`, writes logs to `logs/`, writes pids to `run/`, and fails fast if a process exits immediately after start.

For single-node production with systemd/Nginx/backups, use `docs/single_node_production.md` instead of the local `start_services.sh` workflow.

AdminService is protected by scoped `admin_service.admin_tokens`. Local default config uses development token hashes; replace them for any shared environment.

```text
admin_service.admin_tokens=ops:sha256:<token_sha256_hex>:health,stats,outbox,kafka,cleanup
```

Admin clients must send the raw token through gRPC metadata key `x-nebula-admin-token`. Each AdminService RPC emits an audit log line with action, required scope, principal, and request ID.

Admin cleanup is bounded by config:

```text
admin.cleanup.outbox_published_retention_ms=604800000
admin.cleanup.offline_delivered_retention_ms=604800000
admin.cleanup.friend_request_retention_ms=2592000000
admin.cleanup.message_receipt_retention_ms=7776000000
admin.cleanup.batch_size=1000
```

Use `RunCleanup(dry_run=true)` before destructive cleanup. Cleanup removes expired MySQL rows and prunes stale Redis online device members whose per-device online/connection keys have expired. AdminService also exposes real Redis-derived online user/connection counts and Kafka lag through `GetSystemStats` and `GetKafkaLagInfo`.

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

All C++ gRPC service listeners and internal Gateway/Push clients read these keys. The Gateway TCP/WebSocket listener is still plaintext at the socket layer, so terminate public TLS at the edge unless native Gateway TLS is implemented.

## Database Migration

Migration files live in `deploy/mysql/migration/` and are applied in lexical version order. Applied versions are recorded in `schema_migrations`.

```bash
MYSQL_HOST=127.0.0.1 MYSQL_PORT=3306 MYSQL_USER=nebula MYSQL_PASSWORD=nebula MYSQL_DATABASE=nebula_im ./scripts/migrate_db.sh
```

The script acquires a MySQL named lock before applying migrations, exits on the first failed migration, and skips versions already present in `schema_migrations`. In production mode (`NEBULA_ENV=production`) or when `NEBULA_MIGRATE_BACKUP=true`, it runs `scripts/backup_mysql.sh` before applying new versions. Rollback is an operational restore using `scripts/restore_mysql.sh` or hand-written rollback SQL.

## Readiness And Health

`scripts/wait_ready.sh` waits for the dependencies required by a service. The systemd units call it from `ExecStartPre` after config validation, so service order is not treated as readiness.

```bash
./scripts/wait_ready.sh /etc/nebulaim/nebula.conf gateway
```

`scripts/health_check.sh` checks MySQL query, Redis ping, Kafka metadata, outbox pending/dead status, service ports, and systemd activity when systemd is available.

## Manual Service Startup

```bash
./build/user_service/nebula_user_service --config config/nebula.conf
./build/relation_service/nebula_relation_service --config config/nebula.conf
./build/conversation_service/nebula_conversation_service --config config/nebula.conf
./build/message_service/nebula_message_service --config config/nebula.conf
./build/push_service/nebula_push_service --config config/nebula.conf
./build/gateway/nebula_gateway --config config/nebula.conf
./build/admin_service/nebula_admin_service --config config/nebula.conf
```
