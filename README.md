# NebulaIM

**Language / 语言:** [English](#english) | [中文](#中文)

## English

NebulaIM is a C++17 distributed instant messaging backend. It includes a custom epoll/Reactor TCP and WebSocket Gateway, a binary Packet + Protobuf protocol, gRPC microservices, MySQL persistence, Redis online/token state, Kafka push delivery, Prometheus/Grafana metrics, tracing hooks and production deployment scripts.

### Architecture

```text
Native Client
  -> TCP PacketCodec
  -> Gateway
  -> bounded gRPC RpcExecutor
UserService / RelationService / MessageService / ConversationService / PushService

Browser
  -> Web Bridge /ws
  -> Gateway binary PacketCodec

MessageService
  -> MySQL messages + conversations + outbox_events
  -> OutboxWorker
  -> Kafka
  -> PushService
  -> GatewayService.DeliverToConnection
  -> Gateway

AdminService
  -> health / online stats / outbox stats / Kafka lag / cleanup / audit / config validation
```

### Features

- C++17 epoll + Reactor networking.
- TCP and WebSocket Gateway with the same binary Packet protocol.
- Bounded asynchronous Gateway backend RPC executor.
- User registration, login, token validation, refresh and logout.
- Password hashing and Redis token storage with hashed token keys.
- Username lookup for frontend friend requests.
- RelationService friend requests, friends, groups, group search and group membership.
- MessageService direct/group send, message IDs, deduplication, persistence, conversations, recall and outbox publication.
- ConversationService list/history/read marker support.
- PushService Kafka manual commit after successful handling, retry and DLQ paths.
- Multi-device online state keyed by user, device and Gateway connection.
- Optional internal gRPC metadata auth with `x-nebula-internal-token`.
- Scoped AdminService tokens with `x-nebula-admin-token`.
- Optional gRPC TLS/mTLS and optional native Gateway TLS.
- Prometheus metrics endpoints for Gateway and all services.
- OpenTelemetry OTLP/HTTP trace export hooks.
- MySQL migration, backup, restore, health check and single-node systemd install scripts.

### Quick Start

```bash
./scripts/build.sh
./scripts/start_deps.sh
./scripts/migrate_db.sh
./scripts/init_topics.sh
./scripts/start_services.sh
```

Run a demo client:

```bash
./build/examples/gateway_client_demo --host 127.0.0.1 --port 9000
./build/examples/gateway_websocket_client_demo --url ws://127.0.0.1:9000/
```

Stop services:

```bash
./scripts/stop_services.sh
./scripts/stop_deps.sh
```

### Build

Ubuntu/Debian dependencies:

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config libssl-dev protobuf-compiler libprotobuf-dev protobuf-compiler-grpc libgrpc++-dev default-libmysqlclient-dev libhiredis-dev librdkafka-dev
```

Manual build:

```bash
cmake -S . -B build
cmake --build build -j
```

### Tests

```bash
./scripts/run_tests.sh --unit-only
ctest --test-dir build -L unit --output-on-failure
```

Integration tests require MySQL, Redis and Kafka:

```bash
ctest --test-dir build -L integration --output-on-failure
```

Full backend E2E is opt-in:

```bash
NEBULA_RUN_BACKEND_E2E=1 ./build/tests/test_backend_final_e2e
```

### Service Ports

| Service | Port |
|---|---:|
| Gateway TCP/WebSocket | 9000 |
| Gateway RPC | 50055 |
| UserService | 50051 |
| MessageService | 50052 |
| RelationService | 50053 |
| PushService | 50054 |
| ConversationService | 50056 |
| AdminService | 50057 |
| Prometheus | 9090 |
| Grafana | 3000 |
| Jaeger UI | 16686 |
| OTLP HTTP | 4318 |
| MySQL | 3306 |
| Redis | 6379 |
| Kafka | 9092 |

Metrics exporters:

```text
Gateway 9100
UserService 9101
MessageService 9102
RelationService 9103
PushService 9104
ConversationService 9105
AdminService 9106
```

### Production Notes

Use `config/nebula.prod.conf.example` as a template and replace every `CHANGE_ME` value. Do not commit raw tokens, passwords, private keys or host-specific production config.

Important production settings:

```text
admin_service.admin_tokens=name:sha256:<token_sha256_hex>:scope1,scope2
internal_rpc.auth.enabled=true
internal_rpc.auth.token=<raw-internal-token>
internal_rpc.auth.token_sha256=<sha256-of-raw-internal-token>
mysql.password=<secret>
redis.password=<secret>
```

Validate production config before installing services:

```bash
./scripts/validate_prod_config.sh /etc/nebulaim/nebula.conf
sudo ./scripts/install_single_node.sh
```

See `docs/single_node_production.md` for systemd, Nginx TLS termination, backups, restore and health checks.

### Documentation

Topic documents under `docs/` describe the current backend internals:

- `docs/architecture.md`
- `docs/deployment.md`
- `docs/security_design.md`
- `docs/reliability_design.md`
- `docs/rpc.md`
- `docs/storage.md`
- `docs/user_service.md`
- `docs/message_service.md`
- `docs/relation_service.md`
- `docs/push_service.md`
- `docs/gateway.md`
- `docs/admin_operations.md`
- `docs/production_checklist.md`
- `docs/single_node_production.md`
- `docs/troubleshooting.md`
- `web_sdk/README.md`

### Limits

NebulaIM does not yet include a distributed service discovery cluster backend, Kubernetes Operator, identity-provider-backed admin console, end-to-end encryption, multi-region deployment, or a full OpenTelemetry SDK integration beyond the current trace hooks.

## 中文

NebulaIM 是一个 C++17 分布式即时通信后端。它包含自研 epoll/Reactor TCP 与 WebSocket Gateway、二进制 Packet + Protobuf 协议、gRPC 微服务、MySQL 持久化、Redis 在线状态和 token 状态、Kafka 推送投递、Prometheus/Grafana 指标、追踪钩子以及生产部署脚本。

### 架构

```text
原生客户端
  -> TCP PacketCodec
  -> Gateway
  -> 有界 gRPC RpcExecutor
UserService / RelationService / MessageService / ConversationService / PushService

浏览器
  -> Web Bridge /ws
  -> Gateway 二进制 PacketCodec

MessageService
  -> MySQL messages + conversations + outbox_events
  -> OutboxWorker
  -> Kafka
  -> PushService
  -> GatewayService.DeliverToConnection
  -> Gateway

AdminService
  -> 健康检查 / 在线统计 / outbox 统计 / Kafka 滞后 / 清理 / 审计 / 配置校验
```

### 功能

- C++17 epoll + Reactor 网络库。
- TCP 和 WebSocket Gateway 复用同一套二进制 Packet 协议。
- Gateway 后端 RPC 使用有界异步执行器。
- 用户注册、登录、token 校验、刷新和登出。
- 密码哈希存储，Redis token key 使用 token 哈希。
- 支持按 username 查询用户，用于前端添加好友。
- RelationService 支持好友请求、好友、群组、群搜索和群成员。
- MessageService 支持单聊/群聊发送、消息 ID、去重、持久化、会话更新、撤回和 outbox 发布。
- ConversationService 支持会话列表、历史消息和已读标记。
- PushService 在处理成功后手动提交 Kafka offset，并支持重试和 DLQ 路径。
- 多设备在线状态按 user、device 和 Gateway connection 记录。
- 可选内部 gRPC metadata 鉴权，使用 `x-nebula-internal-token`。
- AdminService 使用 scoped token，metadata key 为 `x-nebula-admin-token`。
- 可选 gRPC TLS/mTLS 和 Gateway 原生 TLS。
- Gateway 和所有服务都有 Prometheus metrics endpoint。
- 支持 OpenTelemetry OTLP/HTTP trace export 钩子。
- 提供 MySQL migration、backup、restore、health check 和单节点 systemd 安装脚本。

### 快速启动

```bash
./scripts/build.sh
./scripts/start_deps.sh
./scripts/migrate_db.sh
./scripts/init_topics.sh
./scripts/start_services.sh
```

运行示例客户端：

```bash
./build/examples/gateway_client_demo --host 127.0.0.1 --port 9000
./build/examples/gateway_websocket_client_demo --url ws://127.0.0.1:9000/
```

停止服务：

```bash
./scripts/stop_services.sh
./scripts/stop_deps.sh
```

### 构建

Ubuntu/Debian 依赖：

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config libssl-dev protobuf-compiler libprotobuf-dev protobuf-compiler-grpc libgrpc++-dev default-libmysqlclient-dev libhiredis-dev librdkafka-dev
```

手动构建：

```bash
cmake -S . -B build
cmake --build build -j
```

### 测试

```bash
./scripts/run_tests.sh --unit-only
ctest --test-dir build -L unit --output-on-failure
```

集成测试需要先启动 MySQL、Redis 和 Kafka：

```bash
ctest --test-dir build -L integration --output-on-failure
```

完整后端 E2E 默认不随普通测试自动运行：

```bash
NEBULA_RUN_BACKEND_E2E=1 ./build/tests/test_backend_final_e2e
```

### 服务端口

| 服务 | 端口 |
|---|---:|
| Gateway TCP/WebSocket | 9000 |
| Gateway RPC | 50055 |
| UserService | 50051 |
| MessageService | 50052 |
| RelationService | 50053 |
| PushService | 50054 |
| ConversationService | 50056 |
| AdminService | 50057 |
| Prometheus | 9090 |
| Grafana | 3000 |
| Jaeger UI | 16686 |
| OTLP HTTP | 4318 |
| MySQL | 3306 |
| Redis | 6379 |
| Kafka | 9092 |

指标端点：

```text
Gateway 9100
UserService 9101
MessageService 9102
RelationService 9103
PushService 9104
ConversationService 9105
AdminService 9106
```

### 生产配置

使用 `config/nebula.prod.conf.example` 作为模板，并替换所有 `CHANGE_ME` 值。不要提交明文 token、密码、私钥或机器专属生产配置。

关键生产设置：

```text
admin_service.admin_tokens=name:sha256:<token_sha256_hex>:scope1,scope2
internal_rpc.auth.enabled=true
internal_rpc.auth.token=<raw-internal-token>
internal_rpc.auth.token_sha256=<sha256-of-raw-internal-token>
mysql.password=<secret>
redis.password=<secret>
```

安装服务前校验生产配置：

```bash
./scripts/validate_prod_config.sh /etc/nebulaim/nebula.conf
sudo ./scripts/install_single_node.sh
```

systemd、Nginx TLS 终止、备份、恢复和健康检查见 `docs/single_node_production.md`。

### 文档

`docs/` 下的专题文档描述当前后端内部实现：

- `docs/architecture.md`
- `docs/deployment.md`
- `docs/security_design.md`
- `docs/reliability_design.md`
- `docs/rpc.md`
- `docs/storage.md`
- `docs/user_service.md`
- `docs/message_service.md`
- `docs/relation_service.md`
- `docs/push_service.md`
- `docs/gateway.md`
- `docs/admin_operations.md`
- `docs/production_checklist.md`
- `docs/single_node_production.md`
- `docs/troubleshooting.md`
- `web_sdk/README.md`

### 限制

NebulaIM 尚未包含分布式服务发现集群后端、Kubernetes Operator、接入身份提供商的管理控制台、端到端加密、多区域部署，以及完整 OpenTelemetry SDK 集成；当前实现只包含轻量 trace hook。
