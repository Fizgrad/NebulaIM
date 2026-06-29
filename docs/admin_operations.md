# Admin Operations

AdminService is the internal operational surface for single-node production.

## Authentication

Admin RPCs require metadata:

```text
x-nebula-admin-token: <raw-token>
```

The config stores only SHA-256 token hashes:

```text
admin_service.admin_tokens=ops:sha256:<token_sha256_hex>:health,stats,outbox,kafka,cleanup
```

Supported scopes are `health`, `stats`, `outbox`, `kafka`, `cleanup`, and `*`.

## RPCs

- `HealthCheck`: checks AdminService and dependencies.
- `GetSystemStats`: counts online users and active device connections from Redis.
- `GetOutboxStats`: groups outbox rows by status.
- `GetKafkaLagInfo`: queries Kafka consumer lag.
- `RunCleanup`: bounded cleanup for published outbox rows, delivered offline rows, handled friend requests, old receipts, and stale Redis online devices.
- `ValidateConfig`: reports production-risk config issues such as missing admin tokens, empty passwords, public Gateway without TLS, or tracing enabled without endpoint.
- `GetServiceOverview`: TCP readiness view for Gateway, gRPC services, and AdminService.
- `ListAuditEvents`: returns recent in-memory allow/deny admin audit events.

## Notes

Audit events are intentionally in-memory for the single-node version. For a larger deployment, persist them to append-only storage or a SIEM pipeline.
