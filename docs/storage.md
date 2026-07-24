# NebulaIM Storage Layer

## Overview

The storage layer provides reusable infrastructure wrappers for MySQL, Redis, and Kafka. Business services should depend on NebulaIM DAO/client interfaces instead of native C APIs.

## Why MySQL

MySQL persists users, relationships, groups, messages, and offline messages. These records require durability, indexing, transactional updates, and SQL query ability.

## Why Redis

Redis stores fast-changing hot data such as tokens, online status, connection mappings, rate limits, recent sessions, and dedup keys. These values need low latency and explicit TTLs.

`RedisClient` serializes commands with an internal mutex because hiredis connections are not safe for concurrent command execution. Gateway EventLoop threads and RPC callbacks can share the client instance without corrupting the Redis connection, but production deployments that need higher Redis throughput should use one client/pool per worker instead of relying on one global connection.

## Why Kafka

Kafka decouples message send, fanout, offline handling, retry, and DLQ processing. It absorbs traffic spikes and lets downstream consumers scale independently.

## MySQL connection pool

`MySqlConnectionPool` creates a fixed number of connections and hands them out via RAII `ConnectionGuard`. It avoids frequent connect/disconnect overhead and supports timeout waiting.

## Runtime storage configuration

All services load MySQL and Redis settings through `StorageConfig`. Values in the configured `nebula.conf` file are the defaults, `NEBULA_MYSQL_*` and `NEBULA_REDIS_*` override them for a deployed process, and `NEBULA_TEST_MYSQL_*` and `NEBULA_TEST_REDIS_*` have the highest priority for an isolated test process. Test-only variables must not be exported by production service units.

Supported MySQL variables are `HOST`, `PORT`, `USER`, `PASSWORD`, and `DATABASE` under the corresponding prefix. Redis supports `HOST`, `PORT`, and `PASSWORD`. This single loader prevents individual services and tests from silently using different credentials.

`MySqlConnection` caches `mysql_affected_rows()` and `mysql_insert_id()` immediately after the statement succeeds, before draining any additional result sets. DAO state-transition checks therefore observe the statement that was just executed instead of metadata from a later result set.

## Transaction wrapper

`MySqlTransaction` starts a transaction on construction. If `commit()` is not called, the destructor rolls back automatically. MessageService uses it to commit `messages`, `conversations`, and `outbox_events` atomically.

## Tables and indexes

- `users.idx_username`: login and username lookup.
- `friendships.uk_user_friend`: dedup friendship edge and quick relation check.
- `friendships.idx_friend_id`: reverse lookup by friend.
- `groups.idx_owner_id`: list groups created by owner.
- `group_members.uk_group_user`: membership dedup and permission checks.
- `group_members.idx_user_id`: list joined groups.
- `messages.idx_conversation_time`: page messages by conversation timeline.
- `messages.idx_from_user_time`: query sent messages by user.
- `messages.idx_to_user_time`: query received single-chat messages.
- `messages.idx_group_time`: query group messages.
- `offline_messages.uk_user_message`: dedup offline message per user.
- `offline_messages.idx_user_status_time`: pull `Pending` and `Pulled` offline messages by user in time order; cleanup deletes only `Acked` rows.
- `friend_requests.uk_from_to_status`: dedup pending friend requests.
- `conversations.uk_owner_conversation`: one conversation view per owner user.
- `outbox_events.idx_status_retry_time`: scan publishable outbox events.
- `user_devices.uk_user_device`: dedup device records per user; `token_hash` stores only the SHA-256 token hash for revocation.
- `message_receipts.uk_message_user`: dedup delivered/read receipt per message and user.

## Redis keys

```text
nebula:token:{sha256(token)} -> user_id, TTL token lifetime
nebula:user:devices:{user_id} -> set(device_id), TTL heartbeat window
nebula:user:online:{user_id}:{device_id} -> gateway_id, TTL heartbeat window
nebula:user:conn:{user_id}:{device_id} -> connection_id, TTL heartbeat window
nebula:rate_limit:user:{user_id} -> counter, TTL one window
nebula:session:recent:{user_id} -> sorted/recent sessions, TTL days
nebula:msg:dedup:{user_id}:{client_sequence_id} -> message_id, TTL dedup window
```

`nebula:user:devices:{user_id}` is authoritative only as a membership set. Readers verify both per-device keys before treating a device as online, prune stale members on PushService reads, and AdminService cleanup can batch-prune stale device members.

If Redis fails, auth should fall back to MySQL only when safe, online status should be treated as unknown/offline, and rate limiting should fail closed or degraded by service policy.

## Kafka topics

```text
nebula.message.single
nebula.message.group
nebula.message.offline
nebula.message.retry
nebula.message.dlq
```

Use `conversation_id` as key where possible so messages for the same conversation tend to land in the same partition. PushService disables Kafka auto commit and commits offsets only after a message is delivered, stored offline, retried, or sent to DLQ successfully. Offset reset, session, heartbeat, and fetch-wait settings are explicit in config to keep startup, restart, and delivery behavior predictable. Consumers must still be idempotent because Kafka can redeliver messages after crashes or retries.

## DAO layer

DAO classes keep SQL in one layer:

- `UserDao`
- `RelationDao`
- `GroupDao`
- `MessageDao`
- `OfflineMessageDao`
- `FriendRequestDao`
- `MessageReceiptDao`
- `ConversationDao`
- `OutboxDao`

This prevents business services from directly assembling SQL and keeps escaping and schema knowledge centralized.

## Docker Compose

```bash
./scripts/start_deps.sh
```

Create Kafka topics:

```bash
./scripts/init_topics.sh
docker exec nebula-kafka /opt/kafka/bin/kafka-topics.sh --bootstrap-server 127.0.0.1:9092 --list
```

## Build dependencies

```bash
sudo apt update
sudo apt install -y default-libmysqlclient-dev libhiredis-dev librdkafka-dev
```

## Demo

```bash
./build/examples/storage_demo
```

## Tests

These tests require MySQL, Redis, and Kafka from Docker Compose:

```bash
./build/tests/test_mysql_pool
./build/tests/test_user_dao
./build/tests/test_message_dao
./build/tests/test_redis_client
./build/tests/test_kafka_producer
./build/tests/test_kafka_consumer
```

## Knowledge checks

1. MySQL needs a connection pool because creating TCP/auth/session state per request is expensive.
2. A pool reuses established connections and controls concurrency.
3. Timeout waiting prevents threads from blocking forever when the pool is exhausted.
4. Redis is suitable for online status because it is low-latency and supports TTL.
5. Online status TTL handles heartbeat expiry and gateway crashes.
6. Gateway crash cleanup can rely on TTL plus startup reconciliation.
7. Outbox Pattern avoids the MySQL/Kafka dual-write gap by committing message data and publish intent in one local transaction.
8. Kafka absorbs spikes and decouples producers from consumers; manual consumer commit reduces the window where a consumed message is lost before delivery handling.
9. Kafka key by conversation keeps same conversation mostly ordered in one partition.
10. Duplicate consumption is handled by message_id and receipt/outbox idempotency in DB/Redis.
11. offline_messages needs `user_id + status + created_at` for offline pull pagination.
12. messages needs `conversation_id + created_at` for conversation history.
13. DAO centralizes SQL and schema mapping.
14. Business code should not directly write SQL because it spreads escaping, schema, and transaction logic everywhere.
