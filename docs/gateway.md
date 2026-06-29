# Gateway

Gateway owns client TCP/WebSocket long connections and exposes GatewayService for PushService. It parses NebulaIM Packet frames, forwards requests to backend gRPC services, stores online state in Redis, and writes PUSH_MSG packets back to connected clients.

## Responsibilities

- TCP accept, WebSocket handshake/frame handling, and connection lifecycle.
- PacketCodec sticky/partial packet decoding.
- Register/Login forwarding to UserService through the RPC executor.
- Message/ACK/offline pull/read/recall forwarding to MessageService through the RPC executor.
- Redis online state write/refresh/delete.
- GatewayService.DeliverToConnection writes PUSH_MSG to the real TcpConnection, wrapping it in a WebSocket binary frame when the client connection is WebSocket.

## Flow

Register: client sends REGISTER_REQ -> Gateway submits UserService.Register to RpcExecutor -> callback returns REGISTER_RESP. Register does not authenticate the connection.

Login: client sends LOGIN_REQ -> Gateway submits UserService.Login to RpcExecutor -> callback returns to the connection EventLoop -> bind user_id/device_id to connection_id -> write Redis online keys -> return LOGIN_RESP.

Heartbeat: update active time -> refresh Redis TTL -> return HEARTBEAT_RESP.

## Production Path

Gateway now supports native TCP Packet clients and browser WebSocket clients on the same TCP port. WebSocket binary frame payloads are NebulaIM PacketCodec bytes, so routing and protobuf handling stay shared.

Backend RPC calls are dispatched through `RpcExecutor`, which runs blocking gRPC stubs in a worker pool and posts responses back to the connection EventLoop. Heartbeat remains a fast local path and does not leave the EventLoop.

Gateway connection context carries `device_id` and `platform`. The production online-state contract is the multi-device Redis key set:

```text
nebula:user:devices:{user_id}
nebula:user:online:{user_id}:{device_id}
nebula:user:conn:{user_id}:{device_id}
```

The older single-device helper methods still exist for a few local tests and transitional tooling, but new production flows should not depend on them.

Message send: require auth -> validate user_id matches connection -> call MessageService -> return response packet.

Push: PushService calls GatewayService.DeliverToConnection -> Gateway finds connection_id -> sends PUSH_MSG packet.

Close: remove local connection state and delete Redis online state if connection_id still matches.

## Design notes

Gateway does not write MySQL and does not consume Kafka. It uses the custom PacketCodec protocol over native TCP or WebSocket for clients and gRPC for internal services. Business forwarding leaves the EventLoop through `RpcExecutor`; a future deeper optimization could replace the worker-pool wrapper with native gRPC async completion queues.

## Run

```bash
./scripts/start_deps.sh
./scripts/migrate_db.sh
./scripts/init_topics.sh
```

```bash
./build/user_service/nebula_user_service --config config/nebula.conf
./build/message_service/nebula_message_service --config config/nebula.conf
./build/push_service/nebula_push_service --config config/nebula.conf
./build/gateway/nebula_gateway --config config/nebula.conf
./build/examples/gateway_client_demo --host 127.0.0.1 --port 9000
./build/examples/gateway_websocket_client_demo --url ws://127.0.0.1:9000/
./build/examples/gateway_push_demo
```

## Tests

```bash
./build/tests/test_connection_manager
./build/tests/test_gateway_packet_helper
./build/tests/test_gateway_online_manager
./build/tests/test_gateway_router
./build/tests/test_gateway_service_impl
./build/tests/test_gateway_client_flow
```

## Interview points

1. Gateway maintains long connections and isolates client protocol from business services.
2. It should not own business persistence.
3. TCP sticky/partial packets are handled by Buffer + PacketCodec.
4. connection_id maps a TCP connection to user_id after login.
5. Repeated login currently overwrites user_to_connection mapping without kicking old connection.
6. Online status has TTL to handle gateway crashes.
7. Heartbeat refreshes local activity and Redis online TTL.
8. PushService routes to the correct gateway using Redis gateway_id and connection_id.
9. Redis and local state may diverge; Push failure or TTL expiry repairs stale online state.
10. Gateway avoids blocking the EventLoop by moving backend RPC work to `RpcExecutor`; native async gRPC would reduce worker-thread blocking further.
