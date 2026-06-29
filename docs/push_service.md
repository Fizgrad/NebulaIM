# PushService

PushService consumes Kafka message topics, checks Redis online status, calls GatewayService for online users, and writes offline messages for offline users.

## Boundaries

MessageService persists and publishes messages. PushService delivers or stores offline messages. Gateway owns client TCP connections and later performs real writes to clients.

## Flow

Single chat: consume `nebula.message.single`, parse MessageData, push to `to_user_id`.

Group chat: consume `nebula.message.group`, list group members, skip sender by default, push each member.

Retry: consume `nebula.message.retry`, retry user/group delivery. Exceeding max retry writes DLQ.

## Redis online status

```text
nebula:user:online:{user_id} -> gateway_id
nebula:user:conn:{user_id} -> connection_id
```

Missing key means offline. TTL will be refreshed by Gateway heartbeat in a later phase.

## Retry and DLQ

Retry count key:

```text
nebula:push:retry:{message_id}:{user_id}
```

Failure below max retry writes retry topic. Failure over max retry writes DLQ and saves offline copy.

## Run

```bash
cd deploy
docker compose up -d
docker compose ps
bash kafka/topics.sh
```

```bash
./build/gateway/nebula_gateway --config ../config/nebula.conf
./build/push_service/nebula_push_service --config ../config/nebula.conf
./build/examples/push_service_client --addr 127.0.0.1:50054
./build/examples/push_kafka_produce_demo
```

## Tests

```bash
./build/tests/test_online_status_manager
./build/tests/test_gateway_client
./build/tests/test_push_retry_manager
./build/tests/test_push_dispatcher
./build/tests/test_push_service_impl
./build/tests/test_push_worker
```

## Interview points

1. PushService consumes Kafka to decouple message acceptance from delivery.
2. MessageService should not directly push because delivery is slow and fanout-heavy.
3. Redis online keys map user to gateway and connection.
4. TTL cleans stale online status after gateway crash.
5. GatewayClientManager maps gateway_id to RPC clients.
6. Retry topic handles transient failures; DLQ captures terminal failures.
7. Offline messages are written when user is offline or final delivery fails.
8. Group fanout belongs here because PushService owns delivery expansion.
9. Large groups require sharded fanout jobs and batching.
10. Kafka consumers may redeliver, so downstream writes should be idempotent.
11. Gateway RPC failure despite Redis online means stale status or gateway failure.
12. Monitor consume lag, push success/failure, retry count, DLQ count, and Gateway RPC latency.
