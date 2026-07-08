# NebulaIM Service Map

Use this map to find the owner of a backend change.

## Gateway And Protocol

- Packet framing: `common/protocol/`, `docs/protocol.md`
- WebSocket transport: `common/websocket/`, `gateway/src/WebSocketGatewayServer.cpp`, `docs/websocket_gateway.md`
- Gateway session and routing: `gateway/`, `common/gateway/`, `docs/gateway.md`
- Protobuf service and message contracts: `proto/`

## Services

- User identity, login, token refresh: `user_service/`, `common/auth/`, `docs/user_service.md`
- Friends, friend requests, groups and group membership: `relation_service/`, `docs/relation_service.md`
- Direct/group messages, recall, receipts, offline pull and outbox writes: `message_service/`, `docs/message_service.md`
- Conversation list, unread counts, pin/mute/delete/read markers: `conversation_service/`, `common/conversation/`, `docs/conversation_design.md`
- Kafka consumption, online delivery, offline storage and retry: `push_service/`, `common/push/`, `docs/push_service.md`
- Admin health, metrics, cleanup, audit and token scopes: `admin_service/`, `docs/admin_operations.md`

## Shared Infrastructure

- MySQL: `common/db/`, `common/dao/`, `deploy/mysql/`, `docs/storage.md`
- Redis: `common/redis/`
- Kafka and outbox: `common/kafka/`, `common/outbox/`, `docs/outbox_pattern.md`
- Config: `common/config/`, `config/`, `docs/deployment.md`
- Metrics: `common/monitor/`, `deploy/prometheus/`, `deploy/grafana/`
- TLS and internal RPC auth: `common/tls/`, `common/rpc/`, `docs/security_design.md`

## Tests

- Shared components: `tests/test_*`
- Service implementation tests: `tests/test_*_service_impl.cpp`
- Integration tests: `tests/test_*_integration.cpp`
- Full backend E2E: `tests/test_backend_final_e2e.cpp`
