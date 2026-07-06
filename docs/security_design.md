# Security Design

Passwords are hashed before storage. Token values live in Redis with TTL and can be explicitly deleted on logout. Redis token keys use `SHA-256(token)` rather than the raw bearer token. `RefreshToken` validates the old token, creates a new token, and invalidates the old token.

Message size is bounded by protocol body length and message content length. WebSocket frame parsing validates mask rules, length encoding, opcodes, and rejects unsupported fragmentation. Browser clients must use binary frames containing NebulaIM Packet bytes; JSON/text frames are not accepted by Gateway.

Logs should avoid full token output; existing code logs short token prefixes. Production deployments should also mask user-provided message payloads in high-volume logs.

Rate limiting protects login, per-user message sending, and per-connection packet throughput. Circuit breakers protect Gateway and PushService from repeatedly calling unhealthy downstream services.

AdminService only accepts scoped admin tokens from gRPC metadata key `x-nebula-admin-token`. Tokens are not part of the business request body.

No default business user is seeded by the MySQL initialization script. Production users should be created through the normal register flow or a controlled internal import tool.

```text
admin_service.admin_tokens=ops:sha256:<token_sha256_hex>:health,stats,outbox;maint:sha256:<token_sha256_hex>:health,cleanup
```

Supported scopes are `health`, `stats`, `outbox`, `kafka`, `cleanup`, and `*`. AdminService writes allow/deny audit log lines with action, scope, principal, and request_id. Config stores SHA-256 token hashes, not raw operational tokens.

Internal business gRPC can also require a shared service token:

```text
internal_rpc.auth.enabled=true
internal_rpc.auth.token=<raw-token-used-by-internal-clients>
internal_rpc.auth.token_sha256=<sha256-of-raw-token>
```

When enabled, Gateway injects `x-nebula-internal-token` for UserService/MessageService calls, PushService injects it for GatewayService calls, and UserService/RelationService/ConversationService/MessageService/PushService/GatewayService reject missing or invalid metadata. This complements loopback binding, firewall rules, and optional gRPC TLS/mTLS.

Trace IDs are propagated through gRPC metadata key `x-nebula-trace-id`. Spans can be exported to Jaeger through OTLP/HTTP. Trace IDs are intentionally not Prometheus labels to avoid high-cardinality metrics.

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

Local development keeps `grpc.tls.enabled=false`. Production service-to-service traffic can enable TLS or mTLS by providing PEM files on every service node.

Gateway client traffic can be protected in two supported ways:

```text
gateway.tls.enabled=true
gateway.tls.cert_path=/etc/nebulaim/tls/gateway.crt
gateway.tls.key_path=/etc/nebulaim/tls/gateway.key
gateway.tls.ca_cert_path=
gateway.tls.require_client_auth=false
```

or by terminating TLS at a load balancer/Nginx/Envoy and forwarding plaintext to a loopback-only Gateway. Native Gateway TLS is implemented in the epoll `TcpConnection` layer, so TCP Packet and WebSocket business parsing remains unchanged after TLS decryption.

`deploy/nginx/nebulaim.conf` includes a WebSocket Origin allowlist, per-IP connection limit, request rate limit, request ID forwarding, and header/body size limits. Replace the example domain/origin before exposing the service.

`web_sdk/nebulaim.js` is the recommended browser boundary because it hides the Packet header and protobuf encoding details from UI code. Frontend code should persist one authenticated WebSocket per logged-in device and use numeric user IDs for message targets.

Still not implemented: end-to-end encryption, full identity-provider backed RBAC, device trust management, and advanced abuse detection.
