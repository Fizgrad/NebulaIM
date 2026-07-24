# MessageService

## Responsibilities

MessageService persists single/group messages, updates conversations, records every Kafka delivery intent through Outbox, handles ACK/read/recall, returns history and offline messages, and owns receipt authorization. It does not maintain client connections or decide online status; PushService consumes Kafka and handles delivery.

## Key flows

SendSingleMessage validates users, content, and the accepted bidirectional friendship before checking the Redis and MySQL idempotency records for `from_user_id + client_sequence_id`. A new send writes `messages`, updates both users' conversations, and inserts `outbox_events` in one transaction. The database unique key resolves concurrent duplicate requests; Redis is only the fast path.

SendGroupMessage applies the same transaction and idempotency rules after validating group existence and membership. Group fanout is deferred to PushService.

Send requests keep the client-provided `content_type`. Text uses `MESSAGE_CONTENT_TYPE_TEXT`; image messages use `MESSAGE_CONTENT_TYPE_IMAGE` with content set to the image URL produced by the web bridge upload API. The persisted row and the Kafka `MessageData` payload carry the same content type so live push, offline pull, and history reads render consistently.

AckMessage is idempotent after authorization: only the direct recipient, or a non-sender group member, can ACK a message. It records delivered receipt state and marks the matching offline row `Acked` if present.

PullOfflineMessages reads `Pending` and `Pulled` rows, parses Protobuf `MessageData`, returns a limited page, and marks returned rows `Pulled`. It does not claim delivery. Only the authorized client ACK records `delivered_at` and changes an offline row to `Acked`, so a response lost before ACK remains pullable.

ListConversationMessages authorizes the requester through the owned conversation and paginates by the stable `(created_at, message_id)` order. The response returns `next_before_time`, `next_before_message_id`, and `has_more`; clients must send both cursor fields when loading the next page so messages sharing one timestamp are neither skipped nor repeated.

## Production Updates

MessageService always writes `messages`, `conversations`, and `outbox_events` in one MySQL transaction. There is no direct Kafka fallback. `OutboxWorker` claims one event with a lease token, waits for the delivery report for that exact Kafka record, and updates the row only while it still owns the claim. Exhausted events become dead only after the DLQ publish succeeds.

Read semantics are split: push/pull is an attempt, ACK records delivery, and `MarkMessageRead` / `MarkConversationRead` record reading. `MarkConversationRead` requires a non-deleted conversation owned by the user and an explicit last-visible message cursor. It marks only messages ordered at or before that cursor and then recalculates unread count. `GetMessageReadState` is available only to the sender.

`RecallMessage` checks sender permission and `message.recall_window_seconds`. The recall update, unread-count recalculation, and recall outbox event are committed in one transaction.

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

A successful send means the message row, conversation updates, and outbox event committed together. Kafka publication remains at-least-once: if Kafka accepts an event but the worker loses its claim before marking it published, the event can be sent again. Stable event/message IDs and idempotent consumers are therefore mandatory. Redis dedup marking is best effort after commit because the MySQL unique key is the authoritative idempotency constraint.

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

## Knowledge checks

1. message_id is globally unique for idempotency, ACK, tracing, and storage lookup.
2. Snowflake combines timestamp, node id, and sequence.
3. Clock rollback can block, fail, or use logical clock; this phase waits.
4. conversation_id groups messages for history queries.
5. Single-chat conversation_id must be independent of sender/receiver order.
6. Persisting before delivery protects message durability.
7. Kafka decouples message acceptance from push delivery.
8. conversation_id key improves per-conversation ordering.
9. MySQL success but Kafka failure is handled by Outbox retry rather than by failing the send API after commit.
10. `client_sequence_id` is protected by a MySQL unique key; Redis only avoids unnecessary database work.
11. ACK is idempotent but still permission-checked because clients may retry ACK.
12. Offline message writing belongs to PushService in this architecture.
13. Large groups should fan out asynchronously and possibly shard fanout jobs.
14. PullOfflineMessages needs paging to avoid huge responses.
