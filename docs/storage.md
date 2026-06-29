# NebulaIM Storage Layer

## Overview

The storage layer provides reusable infrastructure wrappers for MySQL, Redis, and Kafka. Business services should depend on NebulaIM DAO/client interfaces instead of native C APIs.

## Why MySQL

MySQL persists users, relationships, groups, messages, and offline messages. These records require durability, indexing, transactional updates, and SQL query ability.

## Why Redis

Redis stores fast-changing hot data such as tokens, online status, connection mappings, rate limits, recent sessions, and dedup keys. These values need low latency and explicit TTLs.

## Why Kafka

Kafka decouples message send, fanout, offline handling, retry, and DLQ processing. It absorbs traffic spikes and lets downstream consumers scale independently.

## MySQL connection pool

`MySqlConnectionPool` creates a fixed number of connections and hands them out via RAII `ConnectionGuard`. It avoids frequent connect/disconnect overhead and supports timeout waiting.

## Transaction wrapper

`MySqlTransaction` starts a transaction on construction. If `commit()` is not called, destructor rolls back automatically. This will be used later for message + offline-message consistency.

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
- `offline_messages.idx_user_status_time`: pull undelivered offline messages by user in time order.

## Redis keys

```text
nebula:token:{token} -> user_id, TTL token lifetime
nebula:user:online:{user_id} -> gateway_id, TTL heartbeat window
nebula:user:conn:{user_id} -> connection_id, TTL heartbeat window
nebula:rate_limit:user:{user_id} -> counter, TTL one window
nebula:session:recent:{user_id} -> sorted/recent sessions, TTL days
nebula:msg:dedup:{message_id} -> exists, TTL dedup window
```

If Redis fails, auth should fall back to MySQL only when safe, online status should be treated as unknown/offline, and rate limiting should fail closed or degraded by service policy.

## Kafka topics

```text
nebula.message.single
nebula.message.group
nebula.message.offline
nebula.message.retry
nebula.message.dlq
```

Use `conversation_id` as key where possible so messages for the same conversation tend to land in the same partition. Consumers must be idempotent because Kafka can redeliver messages.

## DAO layer

DAO classes keep SQL in one layer:

- `UserDao`
- `RelationDao`
- `GroupDao`
- `MessageDao`
- `OfflineMessageDao`

This prevents business services from directly assembling SQL and keeps escaping and schema knowledge centralized.

## Docker Compose

```bash
cd deploy
docker compose up -d
docker compose ps
```

Create Kafka topics:

```bash
docker exec -it nebula-kafka bash /opt/bitnami/kafka/bin/kafka-topics.sh --bootstrap-server localhost:9092 --list
bash kafka/topics.sh
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

## Interview talking points

1. MySQL needs a connection pool because creating TCP/auth/session state per request is expensive.
2. A pool reuses established connections and controls concurrency.
3. Timeout waiting prevents threads from blocking forever when the pool is exhausted.
4. Redis is suitable for online status because it is low-latency and supports TTL.
5. Online status TTL handles heartbeat expiry and gateway crashes.
6. Gateway crash cleanup can rely on TTL plus startup reconciliation.
7. Write-MySQL-then-Kafka gives durability before delivery; Kafka-first gives lower latency but needs async persistence recovery.
8. Kafka absorbs spikes and decouples producers from consumers.
9. Kafka key by conversation keeps same conversation mostly ordered in one partition.
10. Duplicate consumption is handled by message_id dedup in DB/Redis.
11. offline_messages needs `user_id + status + created_at` for offline pull pagination.
12. messages needs `conversation_id + created_at` for conversation history.
13. DAO centralizes SQL and schema mapping.
14. Business code should not directly write SQL because it spreads escaping, schema, and transaction logic everywhere.
