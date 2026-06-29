# NebulaIM Protocol Layer

## Design goal

The protocol layer defines NebulaIM's client-to-gateway binary packet format. It converts native TCP byte streams or WebSocket binary payloads into complete `Packet` objects and encodes `Packet` objects back to bytes.

`PacketCodec` owns framing only. The packet body is stored as binary bytes and normally contains serialized Protobuf request/response messages from `proto/*.proto`.

## Why transport needs an application protocol

TCP is a reliable byte stream, not a message protocol. One `send` call may be split across multiple `recv` calls, and multiple sends may be merged into one receive buffer. Native TCP clients therefore need explicit packet boundaries.

Browser clients connect through WebSocket. For NebulaIM, WebSocket is only a transport wrapper:

```text
WebSocket binary frame payload = NebulaIM Packet bytes
NebulaIM Packet body = Protobuf bytes
```

Gateway does not accept JSON text frames for business traffic.

## Sticky and partial packets

- Sticky packet: one Buffer contains multiple complete packets.
- Partial packet: one Buffer contains only part of a header or body.

NebulaIM uses a fixed-size header plus variable-size body so the decoder can know exactly when one packet is complete.

## PacketHeader

Header length is fixed at 16 bytes. All integer fields use network byte order.

```cpp
struct PacketHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t type;
    uint32_t sequence_id;
    uint32_t body_length;
};
```

| Field | Meaning |
| --- | --- |
| `magic` | Protocol marker, current value `0x4E494D42` (`NIMB`) |
| `version` | Protocol version, current value `1` |
| `type` | Message type, see `MessageType` |
| `sequence_id` | Request/response correlation ID |
| `body_length` | Body byte length, max `1 MiB` |

Do not directly send or parse the struct memory layout. `encodeHeader` and `decodeHeader` explicitly serialize each field with `htonl/htons/ntohl/ntohs` to avoid padding, alignment, and endian bugs.

## Message types

```text
UNKNOWN = 0
LOGIN_REQ = 1001
LOGIN_RESP = 1002
REGISTER_REQ = 1003
REGISTER_RESP = 1004
HEARTBEAT_REQ = 1101
HEARTBEAT_RESP = 1102
SEND_SINGLE_MSG_REQ = 2001
SEND_SINGLE_MSG_RESP = 2002
SEND_GROUP_MSG_REQ = 2101
SEND_GROUP_MSG_RESP = 2102
PUSH_MSG = 3001
ACK_REQ = 4001
ACK_RESP = 4002
PULL_OFFLINE_MSG_REQ = 5001
PULL_OFFLINE_MSG_RESP = 5002
ERROR_RESP = 9001
```

## Why magic/version/body limit/sequence_id exist

- `magic`: quickly rejects non-NebulaIM traffic or corrupted data.
- `version`: allows future protocol evolution.
- `body_length` limit: prevents malicious oversized packets from exhausting memory.
- `sequence_id`: lets client and gateway match asynchronous responses to requests.

## PacketCodec encode flow

1. Validate message type.
2. Validate body length <= `1 MiB`.
3. Build `PacketHeader`.
4. Encode each header field in network byte order.
5. Append body bytes.

## PacketCodec decode flow

1. If Buffer has fewer than 16 bytes, return `INCOMPLETE_PACKET` without consuming data.
2. Decode header from Buffer peek pointer without retrieving.
3. Validate magic, version, body length, and message type.
4. If body bytes are incomplete, return `INCOMPLETE_PACKET` without consuming data.
5. Retrieve header and body.
6. Fill `Packet` and return `OK`.

## Invalid packet strategy

`INCOMPLETE_PACKET` is normal and keeps Buffer unchanged. Invalid magic, version, type, or body length clears the Buffer and returns an error. Gateway should log the error and close the connection because stream boundaries are no longer trustworthy.

## WebSocket boundary

`WebSocketCodec` parses browser frames, validates mask/length/opcode rules, unmasks the payload, and then passes the binary payload to `PacketCodec`. Server responses and `PUSH_MSG` packets for browser connections are wrapped back into WebSocket binary frames. Native TCP connections receive the same `PacketCodec` bytes without the WebSocket frame wrapper.

## Sticky packet example

```cpp
buffer.append(codec.encode(packet1) + codec.encode(packet2));
while (buffer.readableBytes() >= kPacketHeaderLength) {
    Packet packet;
    ProtocolError err = codec.decode(&buffer, &packet);
    if (err == ProtocolError::OK) {
        handle(packet);
    } else if (err == ProtocolError::INCOMPLETE_PACKET) {
        break;
    } else {
        closeConnection();
        break;
    }
}
```

## Partial packet example

If only 10 header bytes arrive, decode returns `INCOMPLETE_PACKET` and consumes nothing. When the remaining header and body arrive, the same Buffer can decode the full packet.

## Protocol EchoServer

Linux only because it uses the epoll network library.

```bash
cmake -S . -B build
cmake --build build -j
./build/examples/protocol_echo_server
```

Use a custom client or benchmark tool to send a legal NebulaIM Packet to `127.0.0.1:9001`; the server decodes and echoes the same packet.

## Tests

```bash
./build/tests/test_message_type
./build/tests/test_protocol_error
./build/tests/test_packet_codec
```

## Interview talking points

1. TCP is a byte stream, so packet boundaries must be defined by the application.
2. A good application protocol includes a fixed header, body length, type, version, and validation fields.
3. Fixed header + variable body keeps parsing simple while keeping Protobuf parsing outside the framing layer.
4. Network byte order avoids endian incompatibility across machines.
5. Direct struct memcpy is unsafe because of padding, alignment, and endian differences.
6. Magic rejects invalid traffic early.
7. Version supports rolling upgrades and compatibility handling.
8. body_length must be limited to prevent memory exhaustion.
9. sequence_id maps responses to requests in asynchronous clients.
10. Malicious large packets are rejected before body allocation.
11. Multiple packets in one Buffer are handled by decode loops.
12. Half packets remain in Buffer until enough bytes arrive.

## Common questions

### Why clear Buffer on invalid packets?

After invalid header data, stream framing cannot be trusted. Clearing and closing the connection is safer than trying to resynchronize.

### Why not parse Protobuf in PacketCodec?

PacketCodec owns framing only. Protobuf body parsing belongs to the next layer to keep networking, framing, and business serialization decoupled.

### Can body be empty?

Yes. Heartbeat or ACK packets may carry an empty body.
