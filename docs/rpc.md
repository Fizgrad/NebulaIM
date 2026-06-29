# NebulaIM RPC Design

## Why NebulaIM needs RPC

NebulaIM gateway handles long-lived client TCP/WebSocket connections, while user, message, relation, push, and gateway management capabilities are split into backend services. RPC gives these services typed, versioned, and efficient service-to-service communication.

## gRPC and Protobuf roles

- Protobuf defines compact binary message schemas and service contracts.
- gRPC provides client/server stubs, transport, status handling, and service dispatch.

Compared with HTTP JSON, Protobuf is smaller, faster to parse, strongly typed, and easier to evolve with backward-compatible field additions.

## Service architecture

```text
gateway
  | gRPC
  +--> user_service         : Register/Login/ValidateToken/Logout/RefreshToken/GetUserInfo
  +--> message_service      : Send/Ack/PullOffline/Read/Recall
  +--> relation_service     : Friend requests, friendships, groups
  +--> conversation_service : Conversation list and conversation flags

push_service
  | gRPC
  +--> gateway           : DeliverToConnection/KickUser/GetOnlineStatus

admin clients
  | gRPC metadata x-nebula-admin-token
  +--> admin_service     : HealthCheck, cleanup, online stats, outbox stats, Kafka lag
```

## Proto files

- `common.proto`: `CommonResponse`, `UserInfo`, pagination, message data, content/status enums.
- `user.proto`: `UserService` auth and user profile APIs.
- `message.proto`: `MessageService` send, ack, and offline pull APIs.
- `relation.proto`: `RelationService` friend and group APIs.
- `conversation.proto`: `ConversationService` list/read/delete/pin/mute APIs.
- `device.proto`: device management contracts.
- `admin.proto`: internal admin health, cleanup, and stats APIs.
- `push.proto`: `PushService` user/group push APIs.
- `gateway.proto`: `GatewayService` backend-to-gateway delivery APIs.

All response messages contain or directly use `CommonResponse` with `code`, `message`, and `request_id`.

## CMake code generation

`proto/CMakeLists.txt` runs `protoc` with `grpc_cpp_plugin` and emits generated files under `build/generated`. The generated sources are compiled into `nebula_proto`.

Required CMake packages:

```cmake
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
```

If dependencies are missing, this repository keeps non-gRPC phases buildable and skips gRPC targets.

## Dependency installation

Ubuntu/Debian basic Protobuf packages:

```bash
sudo apt update
sudo apt install -y protobuf-compiler libprotobuf-dev
```

gRPC C++ packages vary by distribution. If `find_package(gRPC REQUIRED)` cannot find gRPC, install gRPC from source or a package manager that provides `gRPCConfig.cmake` and `grpc_cpp_plugin`.

## Packet body and Protobuf

Client-to-gateway native TCP uses NebulaIM `PacketCodec` for framing. Browser clients use WebSocket binary frames whose payload is the same PacketCodec byte stream. The packet body is an opaque binary string containing serialized Protobuf for business request/response types.

Gateway wraps blocking backend gRPC calls with `RpcExecutor` so EventLoop threads do not wait on synchronous stubs. The executor queue is bounded, so overload returns `SERVICE_UNAVAILABLE` instead of unbounded task growth. A future optimization can replace this wrapper with native gRPC async completion queues.

PushService uses gRPC `GatewayService.DeliverToConnection` for online delivery. Gateway checks whether the target connection is native TCP or WebSocket and writes either raw Packet bytes or a WebSocket binary frame.

## Examples

Start UserService:

```bash
./build/user_service/nebula_user_service --config config/nebula.conf
```

Run client:

```bash
./build/examples/grpc_user_client
```

Run Packet + Protobuf demo:

```bash
./build/examples/protobuf_packet_demo
```

## Tests

```bash
./build/tests/test_protobuf_message
./build/tests/test_grpc_user_service
```

`test_grpc_user_service` exercises the generated gRPC contract and service implementation without requiring a long-running external process.

## Interview talking points

1. gRPC avoids repeated JSON parse overhead and gives typed service contracts.
2. Protobuf is compact, schema-driven, fast, and backward-compatible when fields are added safely.
3. proto3 removes required fields and has simpler defaults than proto2.
4. gRPC supports unary, server streaming, client streaming, and bidirectional streaming.
5. NebulaIM starts with unary RPC because request/response service calls are enough for initial auth/message/relation APIs.
6. `request_id` enables tracing, idempotency, and response correlation.
7. `CommonResponse` standardizes error handling across services.
8. Packet body is transport payload; Protobuf is payload serialization.
9. Client-to-gateway protocol optimizes long TCP connections; service-to-service gRPC optimizes typed internal RPC.
10. C++ objects cannot be sent directly because memory layout, pointers, alignment, and endian are process-local.

## Common questions

### Why not use HTTP JSON internally?

It is easier to debug but less efficient and less strongly typed. NebulaIM targets high-performance backend communication.

### Why keep PacketCodec separate from Protobuf?

PacketCodec solves TCP framing. Protobuf solves business payload serialization. Separating them keeps the gateway protocol extensible.

### Why keep PacketCodec and gRPC separate?

PacketCodec is the client transport framing protocol for long connections. gRPC is used for service-to-service communication. Keeping them separate prevents browser/native connection concerns from leaking into backend service contracts.
