# Interview Guide

NebulaIM is a C++17 distributed IM system with custom epoll/Reactor networking, binary TCP protocol, gRPC services, MySQL, Redis, Kafka, Prometheus/Grafana, and benchmark tooling.

## Message Flow

Client logs in through Gateway, Gateway writes multi-device Redis online state, client sends message, MessageService persists MySQL/conversation/outbox in one transaction, OutboxWorker publishes Kafka, PushService consumes Kafka with manual commit, checks Redis online state, calls Gateway RPC, and Gateway writes `PUSH_MSG` as raw TCP Packet or WebSocket binary frame.

## High Frequency Q&A

1. Why C++? High performance, network/system programming, memory control.
2. Why epoll + Reactor? Scales long connections with few threads.
3. Why not thread per connection? Too many stacks/context switches.
4. Sticky packets? Buffer + fixed header + body_length.
5. TcpConnection shared_ptr? Callback lifetime safety.
6. Gateway role? Connection/protocol/routing, not business persistence.
7. Online status? Redis keys with TTL.
8. Kafka role? Async delivery, decoupling, peak shaving.
9. Ordering? Kafka key by conversation_id.
10. MySQL success Kafka fail? Outbox Pattern commits publish intent with the message and retries Kafka asynchronously.
11. Dedup? client_sequence_id stored in Redis.
12. ACK idempotent? Retries are common.
13. Large groups? Async sharded fanout.
14. P99 optimization? Reduce blocking, tune DB/Kafka, bound Gateway RPC queues, inspect Kafka lag and outbox backlog.
15. Kafka message loss window? PushService disables auto commit and commits offsets only after delivery/offline/retry/DLQ handling succeeds.
16. Browser access? WebSocket binary frames carry the same PacketCodec bytes; use the Web SDK instead of JSON frames.
17. Production gaps? Native Gateway TLS, distributed service discovery backend, full tracing backend, HA/K8s, CI/CD, E2EE.
