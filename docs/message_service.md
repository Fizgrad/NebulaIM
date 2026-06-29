# MessageService

## Responsibilities

MessageService persists single/group messages, produces Kafka delivery events, handles ACK, and returns offline messages. It does not maintain client connections or decide online status; PushService will consume Kafka and handle delivery.

## Key flows

SendSingleMessage validates users and content, checks Redis dedup by `from_user_id + client_sequence_id`, generates message_id and conversation_id, writes MySQL, produces Kafka `nebula.message.single`, then stores dedup mapping.

SendGroupMessage validates sender, group existence, membership, content, writes MySQL, and produces Kafka `nebula.message.group`. Group fanout is deferred to PushService.

AckMessage is idempotent: it updates message status and marks offline message delivered if present.

PullOfflineMessages reads undelivered offline messages by user, parses Protobuf MessageData payload, returns a limited page, and marks returned messages delivered.

## ID design

`MessageIdGenerator` is a Snowflake-style ID: timestamp, node_id, sequence. If time moves backward, this implementation waits until the last timestamp catches up.

`ConversationId::single(a,b)` sorts users first, so user order does not change the conversation. `ConversationId::group(group_id)` uses a different hash prefix to avoid collision with single chat.

## Kafka

Topics:

```text
nebula.message.single
nebula.message.group
nebula.message.offline
nebula.message.retry
nebula.message.dlq
```

Kafka key is conversation_id so messages in the same conversation tend to enter the same partition.

## Consistency note

Current phase writes MySQL first, then Kafka. If MySQL succeeds but Kafka fails, API returns `MESSAGE_KAFKA_FAILED`; the persisted message remains. Production systems should use Outbox Pattern to reliably publish persisted events.

## Run

```bash
cd deploy
docker compose up -d
docker compose ps
bash kafka/topics.sh
```

```bash
./build/message_service/nebula_message_service --config ../config/nebula.conf
./build/examples/message_service_client --addr 127.0.0.1:50052
```

## Tests

```bash
./build/tests/test_message_id_generator
./build/tests/test_conversation_id
./build/tests/test_message_deduplicator
./build/tests/test_message_service_impl
./build/tests/test_message_service_integration
```

## Interview points

1. message_id is globally unique for idempotency, ACK, tracing, and storage lookup.
2. Snowflake combines timestamp, node id, and sequence.
3. Clock rollback can block, fail, or use logical clock; this phase waits.
4. conversation_id groups messages for history queries.
5. Single-chat conversation_id must be independent of sender/receiver order.
6. Persisting before delivery protects message durability.
7. Kafka decouples message acceptance from push delivery.
8. conversation_id key improves per-conversation ordering.
9. MySQL success but Kafka failure is solved by Outbox Pattern later.
10. client_sequence_id dedups client retries.
11. ACK is idempotent because clients may retry ACK.
12. Offline message writing belongs to PushService in this architecture.
13. Large groups should fan out asynchronously and possibly shard fanout jobs.
14. PullOfflineMessages needs paging to avoid huge responses.
