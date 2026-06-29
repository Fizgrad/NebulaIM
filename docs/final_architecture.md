# Final Architecture

```text
Client -> Gateway(TCP PacketCodec) -> UserService/MessageService(gRPC)
MessageService -> MySQL + Redis dedup + Kafka
Kafka -> PushService -> Redis online -> GatewayService.DeliverToConnection -> Client
```

MySQL persists users, relations, groups, messages and offline messages. Redis stores tokens, online status, dedup and retry counters. Kafka decouples message persistence from push delivery.
