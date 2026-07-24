# WebSocket Gateway

Browsers cannot open arbitrary raw TCP sockets, so native browser clients need WebSocket. NebulaIM now supports both access forms:

```text
Native Client -> TCP Packet -> C++ Gateway
Browser       -> WebSocket Binary Frame -> NebulaIM Packet -> C++ Gateway
```

The Gateway detects a new connection by checking the first bytes. `GET ` enters HTTP Upgrade parsing; otherwise the connection remains native TCP.

`WebSocketHandshake` parses the HTTP request, validates `Upgrade`, `Connection`, `Sec-WebSocket-Version: 13`, extracts `Sec-WebSocket-Key`, computes `Sec-WebSocket-Accept` with SHA1 + Base64, and returns `101 Switching Protocols`. Invalid requests return `400 Bad Request`.

`WebSocketFrame` supports text, binary, ping, pong, close, payload length 126/127, browser mask/unmask, and rejects fragmentation for this phase. Browser frames must be masked; server frames are not masked.

The payload contract is intentionally simple:

```text
WebSocket Binary Payload = NebulaIM PacketCodec bytes
```

That lets frontend code and bridge tools reuse the same PacketCodec semantics. Ping receives automatic pong; close closes the connection. WebSocket responses are wrapped by GatewayServer through the router packet sender hook.

Push delivery also preserves the transport wrapper. `GatewayService.DeliverToConnection` checks the stored connection context; WebSocket clients receive a server WebSocket binary frame containing the `PUSH_MSG` Packet bytes, while native TCP clients receive raw Packet bytes.

Browser applications should not send JSON text frames to Gateway. NebulaIM-Web `DirectGatewayClient` builds:

```text
protobuf request -> NebulaIM Packet -> WebSocket binary frame
```

Common frontend mistakes are sending a display name instead of numeric `to_user_id`, opening a new socket after login, or sending text/JSON frames. Gateway closes non-binary application frames by design.

For public deployment, either terminate TLS at Nginx/Envoy/load balancer or enable native Gateway TLS through `gateway.tls.enabled=true`. `deploy/nginx/nebulaim.conf` includes a WebSocket origin allowlist, per-IP connection/request limits, request ID forwarding, and header/body limits. Replace `nebula.example.com` and allowed origins before production use.

When native Gateway TLS is enabled, browser clients use `wss://host:port/` and native clients use TLS over the same PacketCodec byte stream. TLS is handled below WebSocket/PacketCodec in `TcpConnection`, so frame parsing stays unchanged after decryption.

For pressure tests, count WebSocket handshake cost separately from steady-state binary frame throughput. Keep TLS termination and compression disabled unless explicitly measuring them.
