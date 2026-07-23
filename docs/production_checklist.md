# Production Checklist

## Config

Check ports, service addresses, worker counts, heartbeat timeout, Redis TTLs, token TTLs, rate limits, circuit breaker thresholds, outbox retry settings, Kafka consumer commit/offset/session/heartbeat/fetch-wait settings, Gateway RPC max queue size, and recall window.

Run `./scripts/validate_prod_config.sh /etc/nebulaim/nebula.conf` before starting systemd services on a production host.

Define non-default scoped `admin_service.admin_tokens`. Review `gateway.*.addr` and `*.addr` service resolver entries before multi-instance deployment. Enable `internal_rpc.auth.enabled=true` and set matching `internal_rpc.auth.token` / `internal_rpc.auth.token_sha256` for service-to-service metadata authentication.

Use scoped admin tokens for operations: `health`, `stats`, `outbox`, `kafka`, `cleanup`, or `*`. Store only `name:sha256:<token_sha256_hex>:scopes` entries, and send raw tokens through gRPC metadata key `x-nebula-admin-token`.

For service-to-service encryption, set `grpc.tls.enabled=true` and configure CA, server cert/key, optional client cert/key, `grpc.tls.require_client_auth`, and `grpc.tls.target_name_override`. Confirm every service has readable PEM files before restart.

For direct Gateway TLS, set `gateway.tls.enabled=true`, `gateway.tls.cert_path`, and `gateway.tls.key_path`; otherwise verify Nginx/Envoy is the only public TLS entrypoint.

For tracing, decide whether `trace.enabled=true` should be used on this host and verify Jaeger/OTLP is reachable at `trace.otlp_endpoint`.

Review `admin.cleanup.*` retention windows and `admin.cleanup.batch_size`. `RunCleanup` deletes published outbox rows, acked offline rows, handled friend requests, old message receipts, and stale Redis online device members in bounded batches.

## MySQL

Run `NEBULA_ENV=production ./scripts/migrate_db.sh /etc/nebulaim/nebula.conf`, verify `schema_migrations`, indexes, connection pool size, binlog/backup policy, slow query logging, and disk capacity. The migration script acquires a MySQL named lock and creates a pre-migration backup in production mode. Production initialization must not seed test users.

Enable `scripts/backup_mysql.sh` via cron or a systemd timer and verify restore with `scripts/restore_mysql.sh` in a non-production environment.

## Redis

Verify auth, persistence policy, eviction policy, online TTL keys, hashed token TTL keys, dedup TTL keys, and monitoring.

## Kafka

Run `./scripts/init_topics.sh`, verify single/group/offline/retry/DLQ topics, partition count, retention, consumer group lag, broker health, `kafka.consumer.enable_auto_commit=false`, `kafka.consumer.auto_offset_reset=earliest`, and the interactive defaults `session_timeout_ms=6000`, `heartbeat_interval_ms=2000`, `fetch_wait_max_ms=50` for PushService.

## Gateway

Validate TCP login, WebSocket handshake, WebSocket push delivery, heartbeat, packet size limits, rate limiting, async RPC worker count, `gateway.rpc_max_queue_size`, and connection fd limits.

Validate service discovery entries and circuit breaker behavior before putting Gateway behind a load balancer.

Terminate public TCP/WebSocket TLS at the edge or enable native Gateway TLS. Review the Nginx WebSocket Origin allowlist, per-IP connection limits, request rate limits, and request ID forwarding.

## Monitoring

Check Prometheus scrape targets, Grafana dashboards, Jaeger UI, OTLP endpoint, service metrics endpoints including DeviceService, AdminService `HealthCheck`, `GetSystemStats`, `GetOutboxStats`, `GetKafkaLagInfo`, `GetServiceOverview`, `ValidateConfig`, and alert thresholds.

## Logs

Ensure logs directory exists, rotation is configured, tokens are not fully logged, and trace IDs appear in request paths.

Confirm gRPC clients propagate `x-nebula-trace-id`, internal service clients send `x-nebula-internal-token` when enabled, and Admin clients send `x-nebula-admin-token`.

## Tests And Rollback

Run build, `ctest -L unit`, selected `ctest -L integration` tests, the opt-in full backend E2E, and benchmark. Keep migration rollback SQL or restore plan ready before production schema changes.

For single-node production, also run `./scripts/health_check.sh /etc/nebulaim/nebula.conf` after every deploy.

Full backend E2E command:

```bash
NEBULA_RUN_BACKEND_E2E=1 ./build/tests/test_backend_final_e2e
```
