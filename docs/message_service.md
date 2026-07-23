# MessageService

## Responsibilities

MessageService persists single/group messages, updates conversations, records Kafka delivery intent through Outbox when enabled, handles ACK/read/recall, and returns offline messages. It does not maintain client connections or decide online status; PushService consumes Kafka and handles delivery.

## Key flows

SendSingleMessage validates users, content, and the accepted bidirectional friendship before checking Redis dedup by `from_user_id + client_sequence_id`, generating message_id and conversation_id, writing `messages`, updating both users' conversations, inserting an `outbox_events` row when Outbox is enabled, committing, and then storing the dedup mapping.

SendGroupMessage validates sender, group existence, membership, content, writes `messages`, updates group member conversations, inserts an `outbox_events` row when Outbox is enabled, and commits. Group fanout is deferred to PushService.

Send requests keep the client-provided `content_type`. Text uses `MESSAGE_CONTENT_TYPE_TEXT`; image messages use `MESSAGE_CONTENT_TYPE_IMAGE` with content set to the image URL produced by the web bridge upload API. The persisted row and the Kafka `MessageData` payload carry the same content type so live push, offline pull, and history reads render consistently.

AckMessage is idempotent after authorization: only the direct recipient, or a non-sender group member, can ACK a message. It records delivered receipt state and marks the matching offline row `Acked` if present.

PullOfflineMessages reads `Pending` and `Pulled` offline messages by user, parses Protobuf MessageData payload, returns a limited page, records delivered receipt time, and marks returned offline rows `Pulled`. The row remains until ACK marks it `Acked`, so a client can pull again after a crash before ACK.

## Production Updates

MessageService always writes `messages` and `conversations` in one MySQL transaction for send requests. With Outbox enabled, the same transaction also writes `outbox_events`; Kafka publication moves to `OutboxWorker`, which retries and marks exhausted events dead. With Outbox disabled, MessageService commits the message/conversation transaction first and then publishes directly to Kafka.

Read semantics are split: pull/push means delivered, ACK confirms the client accepted the message, and `MarkMessageRead` / `MarkConversationRead` update read state. `MarkMessageRead` requires the user to be the recipient or a non-sender group member. `MarkConversationRead` requires a non-deleted conversation row owned by that user. Read operations clear the corresponding conversation unread count for that user.

`RecallMessage` checks sender permission and `message.recall_window_seconds`. When Outbox is enabled, the message recall update and recall outbox event are committed in the same MySQL transaction, so clients do not end up with a recalled row that has no corresponding push event.

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

With Outbox enabled, MessageService no longer depends on Kafka success in the user request path. A successful send means the message row, conversation updates, and outbox event were committed in one MySQL transaction. A successful recall means the recall state and recall event were committed together. `OutboxWorker` later publishes the event to Kafka and retries with backoff on transient failures. With Outbox disabled, direct Kafka publication happens after the DB commit and a publish failure is returned as `MESSAGE_KAFKA_FAILED`; the durable message/conversation rows remain. Redis dedup marking happens after commit and is best effort: a dedup write failure is logged, not returned as a failed send. Consumers must remain idempotent because Kafka can redeliver.

## Run

```bash
./scripts/start_deps.sh
./scripts/migrate_db.sh
./scripts/init_topics.sh
```

```bash
./build/message_service/nebula_message_service --config config/nebula.conf
./build/examples/message_service_client --addr 127.0.0.1:50052
```

## Tests

```bash
./build/tests/test_message_id_generator
./build/tests/test_conversation_id
./build/tests/test_message_deduplicator
./build/tests/test_message_service_impl
./build/tests/test_message_service_integration
NEBULA_RUN_BACKEND_E2E=1 ./build/tests/test_backend_final_e2e
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
9. MySQL success but Kafka failure is handled by Outbox retry rather than by failing the send API after commit.
10. client_sequence_id dedups client retries when the Redis dedup key is available; send success is not rolled back if the post-commit dedup mark fails.
11. ACK is idempotent but still permission-checked because clients may retry ACK.
12. Offline message writing belongs to PushService in this architecture.
13. Large groups should fan out asynchronously and possibly shard fanout jobs.
14. PullOfflineMessages needs paging to avoid huge responses.
