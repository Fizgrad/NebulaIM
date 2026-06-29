# Gateway

Gateway owns client TCP long connections and exposes GatewayService for PushService. It parses NebulaIM Packet frames, forwards requests to backend gRPC services, stores online state in Redis, and writes PUSH_MSG packets back to TCP clients.

## Responsibilities

- TCP accept and connection lifecycle.
- PacketCodec sticky/partial packet decoding.
- Login forwarding to UserService.
- Message/ACK/offline pull forwarding to MessageService.
- Redis online state write/refresh/delete.
- GatewayService.DeliverToConnection writes PUSH_MSG to the real TcpConnection.

## Flow

Login: client sends LOGIN_REQ -> Gateway calls UserService.Login -> bind user_id to connection_id -> write Redis online keys -> return LOGIN_RESP.

Heartbeat: update active time -> refresh Redis TTL -> return HEARTBEAT_RESP.

Message send: require auth -> validate user_id matches connection -> call MessageService -> return response packet.

Push: PushService calls GatewayService.DeliverToConnection -> Gateway finds connection_id -> sends PUSH_MSG packet.

Close: remove local connection state and delete Redis online state if connection_id still matches.

## Design notes

Gateway does not write MySQL and does not consume Kafka. It uses custom TCP protocol for clients and gRPC for internal services. Current router uses synchronous gRPC calls inside the EventLoop; later phases can move business forwarding to a worker pool or async gRPC.

## Run

```bash
cd deploy
docker compose up -d
docker compose ps
bash kafka/topics.sh
```

```bash
./build/user_service/nebula_user_service --config ../config/nebula.conf
./build/message_service/nebula_message_service --config ../config/nebula.conf
./build/push_service/nebula_push_service --config ../config/nebula.conf
./build/gateway/nebula_gateway --config ../config/nebula.conf
./build/examples/gateway_client_demo --host 127.0.0.1 --port 9000
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
10. Sync gRPC in EventLoop is simple but can block; async RPC or worker pool is a future optimization.
