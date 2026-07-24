# PushService

PushService consumes Kafka message topics, checks Redis online status, calls GatewayService for online users, and writes offline messages for offline users. Kafka auto commit is disabled; offsets are committed only after the message is handled successfully.

## Boundaries

MessageService persists messages and records Kafka publication intent through Outbox. PushService consumes published events, delivers or stores offline messages, and Gateway owns client TCP/WebSocket connections.

## Flow

Single chat: consume `nebula.message.single`, parse MessageData, push to `to_user_id`.

Group chat: consume `nebula.message.group`, list group members, skip sender by default, push each member.

Retry: consume `nebula.message.retry`, retry user/group delivery. Exceeding max retry writes DLQ and/or offline storage. If handling fails before that point, the consumer does not commit the offset so Kafka can redeliver.

## Redis online status

```text
nebula:user:devices:{user_id} -> set(device_id)
nebula:user:online:{user_id}:{device_id} -> gateway_id
nebula:user:conn:{user_id}:{device_id} -> connection_id
```

Missing device keys mean that device is offline. Gateway refreshes TTL on heartbeat and removes the current device mapping on close/logout. Push-side online reads prune stale device IDs from `nebula:user:devices:{user_id}` when either the gateway or connection key has expired.

PushService defaults to all online devices for a user. The production online-state model uses only the multi-device keys above; old single-user Redis keys are not written or read by the production path.

When PushService delivers to a browser connection, GatewayService wraps the `PUSH_MSG` packet in a WebSocket binary frame before writing to the socket. Native TCP clients still receive raw NebulaIM Packet bytes.

## Kafka consumer config

PushService reads these settings from config:

```text
kafka.consumer.enable_auto_commit=false
kafka.consumer.auto_offset_reset=earliest
kafka.consumer.session_timeout_ms=6000
kafka.consumer.heartbeat_interval_ms=2000
kafka.consumer.max_poll_interval_ms=300000
kafka.consumer.fetch_wait_max_ms=50
```

`auto_offset_reset=earliest` is used only when the consumer group has no committed offset. The short session timeout and heartbeat interval reduce the window where a restarted PushService waits for an old consumer-group member to expire. `fetch_wait_max_ms=50` keeps broker-side fetch waits low for interactive message delivery. Keep `heartbeat_interval_ms` lower than `session_timeout_ms`.

## Retry and DLQ

Retry count key:

```text
nebula:push:retry:{message_id}:{user_id}
```

Failure below max retry writes retry topic. Failure over max retry writes DLQ and saves offline copy. The Kafka offset is committed only after this handling step succeeds.

## Run

```bash
./scripts/start_deps.sh
./scripts/migrate_db.sh
./scripts/init_topics.sh
```

```bash
./build/gateway/nebula_gateway --config config/nebula.conf
./build/push_service/nebula_push_service --config config/nebula.conf
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

## Knowledge checks

1. PushService consumes Kafka to decouple message acceptance from delivery.
2. MessageService should not directly push because delivery is slow and fanout-heavy.
3. Redis online keys map user and device to gateway and connection.
4. TTL cleans stale online status after gateway crash.
5. GatewayClientManager maps gateway_id to RPC clients.
6. Retry topic handles transient failures; DLQ captures terminal failures.
7. Offline messages are written when user is offline or final delivery fails.
8. Group fanout belongs here because PushService owns delivery expansion.
9. Large groups require sharded fanout jobs and batching.
10. Kafka consumers commit offsets after successful handling; redelivery is still possible, so downstream writes should be idempotent.
11. Gateway RPC failure despite Redis online means stale status or gateway failure.
12. Monitor consume lag, push success/failure, retry count, DLQ count, and Gateway RPC latency.
