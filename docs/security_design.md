# Security Design

Passwords are hashed before storage. Token values live in Redis with TTL and can be explicitly deleted on logout. `RefreshToken` validates the old token, creates a new token, and invalidates the old token.

Message size is bounded by protocol body length and message content length. WebSocket frame parsing validates mask rules, length encoding, opcodes, and rejects unsupported fragmentation.

Logs should avoid full token output; existing code logs short token prefixes. Production deployments should also mask user-provided message payloads in high-volume logs.

Rate limiting protects login, per-user message sending, and per-connection packet throughput. Circuit breakers protect Gateway and PushService from repeatedly calling unhealthy downstream services.

AdminService only accepts scoped admin tokens from gRPC metadata key `x-nebula-admin-token`. Tokens are not part of the business request body.

```text
admin_service.admin_tokens=ops:sha256:<token_sha256_hex>:health,stats,outbox;maint:sha256:<token_sha256_hex>:health,cleanup
```

Supported scopes are `health`, `stats`, `outbox`, `kafka`, `cleanup`, and `*`. AdminService writes allow/deny audit log lines with action, scope, principal, and request_id. Config stores SHA-256 token hashes, not raw operational tokens.

Trace IDs are propagated through gRPC metadata key `x-nebula-trace-id`. They are intentionally not Prometheus labels to avoid high-cardinality metrics.

gRPC TLS/mTLS is config-driven through:

```text
grpc.tls.enabled
grpc.tls.ca_cert_path
grpc.tls.server_cert_path
grpc.tls.server_key_path
grpc.tls.client_cert_path
grpc.tls.client_key_path
grpc.tls.require_client_auth
grpc.tls.target_name_override
```

Local development keeps `grpc.tls.enabled=false`. Production service-to-service traffic can enable TLS or mTLS by providing PEM files on every service node. Public TCP/WebSocket Gateway TLS is still best terminated at a load balancer/Nginx/Envoy in the current codebase; native TLS in the epoll socket layer remains future work.

Still not implemented: end-to-end encryption, full identity-provider backed RBAC, device trust management, and advanced abuse detection.
