# Gateway

Gateway owns client TCP/WebSocket long connections and exposes GatewayService for PushService. It parses NebulaIM Packet frames, forwards requests to backend gRPC services, stores online state in Redis, and writes PUSH_MSG packets back to connected clients.

## Responsibilities

- TCP accept, WebSocket handshake/frame handling, and connection lifecycle.
- PacketCodec sticky/partial packet decoding.
- Register/Login forwarding to UserService through the RPC executor.
- Message/ACK/offline pull forwarding to MessageService through the RPC executor.
- Redis online state write/refresh/delete.
- GatewayService.DeliverToConnection writes PUSH_MSG to the real TcpConnection, wrapping it in a WebSocket binary frame when the client connection is WebSocket.

## Flow

Register: client sends REGISTER_REQ -> Gateway submits UserService.Register to RpcExecutor -> callback returns REGISTER_RESP. Register does not authenticate the connection.

Login: client sends LOGIN_REQ -> Gateway submits UserService.Login to RpcExecutor -> callback returns to the connection EventLoop -> bind user_id/device_id to connection_id -> write Redis online keys -> return LOGIN_RESP.

Heartbeat: update active time -> refresh Redis TTL -> return HEARTBEAT_RESP.

## Production Path

Gateway now supports native TCP Packet clients and browser WebSocket clients on the same TCP port. WebSocket binary frame payloads are NebulaIM PacketCodec bytes, so routing and protobuf handling stay shared.

The listener can optionally terminate TLS directly:

```text
gateway.tls.enabled=true
gateway.tls.cert_path=/etc/nebulaim/tls/gateway.crt
gateway.tls.key_path=/etc/nebulaim/tls/gateway.key
gateway.tls.ca_cert_path=
gateway.tls.require_client_auth=false
```

When enabled, the same Gateway port accepts TLS-protected TCP Packet clients and TLS-protected WebSocket clients (`wss://`). Nginx/Envoy TLS termination is still a valid production deployment, especially when central certificate automation and edge rate limiting are preferred.

Backend RPC calls are dispatched through `RpcExecutor`, which runs blocking gRPC stubs in a worker pool and posts responses back to the connection EventLoop. The executor queue is bounded by `gateway.rpc_max_queue_size`; if it is saturated, Gateway returns `SERVICE_UNAVAILABLE` instead of growing memory without bound. Heartbeat remains a fast local path and does not leave the EventLoop.

Gateway connection context carries `device_id` and `platform`. The local connection index is `user_id + device_id -> connection_id`; there is no single `user_id -> connection_id` production index. The Redis online-state contract is:

```text
nebula:user:devices:{user_id}
nebula:user:online:{user_id}:{device_id}
nebula:user:conn:{user_id}:{device_id}
```

Gateway no longer writes or reads old single-user Redis online keys. Production online state is device-scoped only.

Gateway writes online state from multiple EventLoop/RPC callback paths. The shared `RedisClient` serializes hiredis commands internally, so online writes, heartbeat refreshes, and disconnect cleanup cannot corrupt one Redis connection. If a deployment raises Gateway worker concurrency materially, prefer a Redis client pool to avoid that mutex becoming a throughput bottleneck.

Message send: require auth -> validate user_id matches connection -> call MessageService -> return response packet.

Push: PushService calls GatewayService.DeliverToConnection -> Gateway finds connection_id -> sends PUSH_MSG packet. WebSocket connections are recorded in `ConnectionContext`, so Gateway RPC delivery wraps push packets in WebSocket binary frames for browser clients and leaves native TCP clients on raw Packet bytes.

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

## Knowledge checks

1. Gateway maintains long connections and isolates client protocol from business services.
2. It should not own business persistence.
3. TCP sticky/partial packets are handled by Buffer + PacketCodec.
4. connection_id maps a TCP connection to user_id after login.
5. Multi-device login records device_id/platform and Redis stores one online mapping per device.
6. Online status has TTL to handle gateway crashes.
7. Heartbeat refreshes local activity and Redis online TTL.
8. PushService routes to the correct gateway using Redis gateway_id and connection_id.
9. Redis and local state may diverge; Push failure or TTL expiry repairs stale online state.
10. Gateway avoids blocking the EventLoop by moving backend RPC work to `RpcExecutor`; the bounded queue provides backpressure, while native async gRPC would reduce worker-thread blocking further.
11. Native TLS is implemented at `TcpConnection`, so PacketCodec/WebSocket business code stays unchanged after decryption.
