# Outbox Pattern

The original MessageService path could write MySQL successfully and then fail Kafka produce. That creates a dual-write consistency gap: the message is durable but PushService never sees it.

NebulaIM now uses Outbox Pattern for the main message and recall publication paths:

```text
begin transaction
insert messages
upsert conversations
insert outbox_events
commit
return success
OutboxWorker scans and publishes Kafka asynchronously
```

`outbox_events` stores topic, key, payload, status, retry count, next retry time, aggregate identity, and trace ID. Status values are pending, published, failed, dead, and an internal claimed/leased state used while a worker is publishing.

`OutboxWorker` claims pending/failed events whose retry time has arrived, using `FOR UPDATE SKIP LOCKED` and a short lease to prevent normal duplicate publication by multiple workers. Success marks the event published. Failure increments retry count and schedules backoff. Events exceeding max retry are marked dead and can be sent to the DLQ topic.

Message send commits `messages`, `conversations`, and `outbox_events` together. Message recall commits `messages.recalled/recalled_at` and the recall outbox event together. That prevents durable user-visible state from diverging from the event stream that PushService consumes.

PushService consumers disable Kafka auto commit and commit offsets only after handling succeeds. Outbox handles producer-side MySQL/Kafka consistency; manual consumer commit handles consumer-side crash windows.

Interview framing: Outbox does not make MySQL and Kafka a single atomic transaction; it makes the local transaction authoritative and turns Kafka publishing into an eventually consistent, retryable process. Consumers must still be idempotent.
