# NebulaIM Web SDK

This SDK is the browser-side boundary for the native C++ Gateway WebSocket protocol.

Gateway expects:

```text
WebSocket binary frame payload = NebulaIM Packet bytes
NebulaIM Packet body = protobuf bytes
```

It does not accept JSON text frames. The SDK wraps:

- WebSocket connection lifecycle.
- NebulaIM packet header encode/decode.
- Register/login/send/ack/pull-offline helpers.
- Heartbeat.
- Push message callback.
- Request timeout/error handling.

The application must provide protobuf message classes generated from `proto/*.proto`, for example with `protobufjs`.

```js
import { NebulaIMClient } from './nebulaim.js';
import { proto } from './generated/nebula_proto.js';

const client = new NebulaIMClient({
  url: 'wss://nebula.example.com/ws',
  proto: proto.nebula.proto,
});

client.onPush = (message) => {
  console.log('push', message);
};

await client.connect();
await client.register({ username: 'alice', password: 'password123', nickname: 'Alice' });
const login = await client.login({ username: 'alice', password: 'password123' });
await client.sendSingleMessage({ toUserId: 10002, content: 'hello' });
```

If the frontend sees `failed` on message send, inspect the decoded response code first. Common causes are:

- The WebSocket was not logged in before sending.
- `toUserId` is a numeric user id, not a display name.
- The frontend sent JSON or a text frame instead of protobuf-in-packet binary data.
- The request used a different socket from the one that logged in.
