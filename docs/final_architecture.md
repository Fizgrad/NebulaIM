# Final Architecture

```text
Native Client -> TCP/TLS PacketCodec ------------------+
Browser -> Web Bridge /ws -> TCP/TLS PacketCodec ------+--> Gateway
                                                             |
                                                             | bounded gRPC/RpcExecutor
                                                             v
UserService / RelationService / ConversationService / MessageService / DeviceService
                                                             |
                                                             v
                                                          MySQL + Redis
                                                             |
                                                             | messages + conversations + outbox_events transaction
                                                             v
                                                       OutboxWorker -> Kafka
                                                                          |
                                                                          v
                                                                    PushService
                                                                          |
                                                                          | manual commit after handling
                                                                          v
                                                       GatewayService.DeliverToConnection
                                                                          |
                                                                          v
                                                        TCP Packet or WebSocket binary PUSH_MSG
```

AdminService is internal and token-protected. It reports dependency health, online users, active device connections, outbox counts, Kafka lag, config validation, service overview, recent audit events, and runs bounded cleanup tasks.

MySQL persists users, devices, friend requests, friendships, groups, messages, conversations, receipts, offline messages, outbox events, and schema migrations. Redis stores tokens, multi-device online status, rate-limit state, retry counters, and dedup keys. Kafka decouples accepted messages from push delivery and retry/DLQ handling. Trace spans can be exported through OTLP/HTTP to Jaeger.
