# Production Checklist

## Config

Check ports, service addresses, worker counts, heartbeat timeout, Redis TTLs, token TTLs, rate limits, circuit breaker thresholds, outbox retry settings, and recall window.

Run `./scripts/validate_prod_config.sh /etc/nebulaim/nebula.conf` before starting systemd services on a production host.

Define non-default scoped `admin_service.admin_tokens`. Review `gateway.*.addr` and `*.addr` service resolver entries before multi-instance deployment.

Use scoped admin tokens for operations: `health`, `stats`, `outbox`, `kafka`, `cleanup`, or `*`. Store only `name:sha256:<token_sha256_hex>:scopes` entries, and send raw tokens through gRPC metadata key `x-nebula-admin-token`.

For service-to-service encryption, set `grpc.tls.enabled=true` and configure CA, server cert/key, optional client cert/key, `grpc.tls.require_client_auth`, and `grpc.tls.target_name_override`. Confirm every service has readable PEM files before restart.

Review `admin.cleanup.*` retention windows and `admin.cleanup.batch_size`. `RunCleanup` deletes published outbox rows, delivered offline rows, handled friend requests, and old message receipts in bounded batches.

## MySQL

Run `./scripts/migrate_db.sh`, verify `schema_migrations`, indexes, connection pool size, binlog/backup policy, slow query logging, and disk capacity.

Enable `scripts/backup_mysql.sh` via cron or a systemd timer and verify restore with `scripts/restore_mysql.sh` in a non-production environment.

## Redis

Verify auth, persistence policy, eviction policy, online TTL keys, token TTL keys, dedup TTL keys, and monitoring.

## Kafka

Run `./scripts/init_topics.sh`, verify single/group/offline/retry/DLQ topics, partition count, retention, consumer group lag, and broker health.

## Gateway

Validate TCP login, WebSocket handshake, heartbeat, packet size limits, rate limiting, async RPC worker count, and connection fd limits.

Validate service discovery entries and circuit breaker behavior before putting Gateway behind a load balancer.

Terminate public TCP/WebSocket TLS at the edge unless native Gateway socket TLS has been added and pressure-tested.

## Monitoring

Check Prometheus scrape targets, Grafana dashboards, service metrics endpoints, and alert thresholds.

## Logs

Ensure logs directory exists, rotation is configured, tokens are not fully logged, and trace IDs appear in request paths.

Confirm gRPC clients propagate `x-nebula-trace-id` and Admin clients send `x-nebula-admin-token`.

## Tests And Rollback

Run build, ctest, selected integration tests, and benchmark. Keep migration rollback SQL or restore plan ready before production schema changes.

For single-node production, also run `./scripts/health_check.sh /etc/nebulaim/nebula.conf` after every deploy.
