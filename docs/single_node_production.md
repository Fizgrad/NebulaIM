# Single Node Production Deployment

This guide targets one Linux server running NebulaIM C++ services under systemd, with MySQL/Redis/Kafka/Prometheus/Grafana under Docker Compose. Public WebSocket TLS is terminated by Nginx and forwarded to the local Gateway.

## Server Layout

```text
/opt/nebulaim/              installed binaries, docs, deploy assets
/etc/nebulaim/nebula.conf   production config
/var/log/nebulaim/          service logs
/var/lib/nebulaim/          local runtime data
/var/backups/nebulaim/      MySQL backups
```

Only Nginx should listen publicly on 80/443. Internal gRPC, MySQL, Redis, Kafka, Prometheus, and Grafana should bind to `127.0.0.1`.

## Build

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Secrets

Generate admin token hashes:

```bash
./scripts/render_admin_token_hash.sh 'replace-with-long-random-token'
```

Set dependency passwords before starting production Compose:

```bash
export NEBULA_MYSQL_ROOT_PASSWORD='replace-me'
export NEBULA_MYSQL_PASSWORD='replace-me'
export NEBULA_REDIS_PASSWORD='replace-me'
export NEBULA_GRAFANA_PASSWORD='replace-me'
```

## Dependencies

```bash
./scripts/start_deps_prod.sh
NEBULA_ENV=production MYSQL_PASSWORD="$NEBULA_MYSQL_PASSWORD" ./scripts/migrate_db.sh
./scripts/init_topics.sh
```

`deploy/docker-compose.prod.yml` binds middleware ports to loopback and requires non-empty passwords through environment variables. In production mode the migration script takes a pre-migration backup and acquires a MySQL named lock before applying versioned SQL.

## Config

Install the example and edit it:

```bash
sudo mkdir -p /etc/nebulaim
sudo cp config/nebula.prod.conf.example /etc/nebulaim/nebula.conf
sudo editor /etc/nebulaim/nebula.conf
./scripts/validate_prod_config.sh /etc/nebulaim/nebula.conf
```

The production validator fails on bundled development admin token hashes, `CHANGE_ME` values, empty Redis password, default MySQL password, and public internal RPC bindings without gRPC TLS.

## Install Services

```bash
sudo ./scripts/install_single_node.sh
sudo systemctl start nebulaim.target
sudo systemctl status nebulaim.target
```

The service units run `validate_prod_config.sh` and `wait_ready.sh` from `ExecStartPre`, so dependencies must be reachable before each service starts.

Useful commands:

```bash
sudo systemctl restart nebula-gateway.service
sudo journalctl -u nebula-gateway.service -f
./scripts/health_check.sh /etc/nebulaim/nebula.conf
```

## Nginx TLS

Copy `deploy/nginx/nebulaim.conf` to your Nginx site directory, replace `nebula.example.com`, update the WebSocket Origin allowlist, and issue certificates with your preferred ACME client. The WebSocket entry forwards `/ws` to `127.0.0.1:9000` and includes per-IP connection/request limits, request ID forwarding, and header/body limits.

For native TCP clients, use the optional `deploy/nginx/nebulaim-stream.conf.example` inside the Nginx `stream {}` context and expose a TLS TCP port such as `9443`.

## Backups

Run a manual backup:

```bash
MYSQL_PASSWORD="$NEBULA_MYSQL_PASSWORD" sudo -E ./scripts/backup_mysql.sh
```

Example cron:

```cron
15 3 * * * MYSQL_PASSWORD=replace-me /opt/nebulaim/scripts/backup_mysql.sh >/var/log/nebulaim/mysql_backup.log 2>&1
```

Restore is intentionally guarded:

```bash
NEBULA_RESTORE_CONFIRM=yes MYSQL_PASSWORD="$NEBULA_MYSQL_PASSWORD" ./scripts/restore_mysql.sh /var/backups/nebulaim/mysql/nebula_im_YYYYmmdd_HHMMSS.sql.gz
```

## Firewall

Allow only:

```text
22/tcp     SSH
80/tcp     ACME redirect
443/tcp    WebSocket over TLS
9443/tcp   optional native TCP over TLS
```

Do not expose MySQL, Redis, Kafka, gRPC service ports, Prometheus, Grafana, or the raw Gateway port directly to the internet.

## Rollout Checklist

1. Build and ctest pass on the target release.
2. Production config validates.
3. Middleware is bound to loopback.
4. Migrations have been applied.
5. Kafka topics exist.
6. systemd services are active.
7. Nginx TLS certificate is valid.
8. `health_check.sh` passes locally.
9. Backup job has produced at least one restorable dump.
10. `NEBULA_RUN_BACKEND_E2E=1 ./build/tests/test_backend_final_e2e` passes against the running services.

`health_check.sh` performs semantic checks for MySQL query, Redis ping, Kafka metadata, outbox pending/dead rows, service ports, and systemd status when available.
