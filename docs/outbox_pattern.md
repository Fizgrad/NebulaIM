# Outbox Pattern

The original MessageService path could write MySQL successfully and then fail Kafka produce. That creates a dual-write consistency gap: the message is durable but PushService never sees it.

NebulaIM now uses Outbox Pattern for the main message path:

```text
begin transaction
insert messages
upsert conversations
insert outbox_events
commit
return success
OutboxWorker scans and publishes Kafka asynchronously
```

`outbox_events` stores topic, key, payload, status, retry count, next retry time, aggregate identity, and trace ID. Status values are pending, published, failed, and dead.

`OutboxWorker` scans pending/failed events whose retry time has arrived. Success marks the event published. Failure increments retry count and schedules backoff. Events exceeding max retry are marked dead and can be sent to the DLQ topic.

Interview framing: Outbox does not make MySQL and Kafka a single atomic transaction; it makes the local transaction authoritative and turns Kafka publishing into an eventually consistent, retryable process. Consumers must still be idempotent.
