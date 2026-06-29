export const MessageType = Object.freeze({
  LOGIN_REQ: 1001,
  LOGIN_RESP: 1002,
  REGISTER_REQ: 1003,
  REGISTER_RESP: 1004,
  HEARTBEAT_REQ: 1101,
  HEARTBEAT_RESP: 1102,
  SEND_SINGLE_MSG_REQ: 2001,
  SEND_SINGLE_MSG_RESP: 2002,
  SEND_GROUP_MSG_REQ: 2101,
  SEND_GROUP_MSG_RESP: 2102,
  PUSH_MSG: 3001,
  ACK_REQ: 4001,
  ACK_RESP: 4002,
  PULL_OFFLINE_MSG_REQ: 5001,
  PULL_OFFLINE_MSG_RESP: 5002,
  ERROR_RESP: 9001,
});

const MAGIC = 0x4e494d31;
const VERSION = 1;
const HEADER_LENGTH = 16;

export class PacketCodec {
  encode(type, sequenceId, body) {
    const payload = body instanceof Uint8Array ? body : new Uint8Array(body || []);
    const out = new Uint8Array(HEADER_LENGTH + payload.length);
    const view = new DataView(out.buffer);
    view.setUint32(0, MAGIC, false);
    view.setUint16(4, VERSION, false);
    view.setUint16(6, type, false);
    view.setUint32(8, sequenceId >>> 0, false);
    view.setUint32(12, payload.length >>> 0, false);
    out.set(payload, HEADER_LENGTH);
    return out;
  }

  decode(buffer) {
    const data = buffer instanceof Uint8Array ? buffer : new Uint8Array(buffer);
    if (data.byteLength < HEADER_LENGTH) throw new Error('incomplete packet header');
    const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
    if (view.getUint32(0, false) !== MAGIC) throw new Error('invalid NebulaIM packet magic');
    if (view.getUint16(4, false) !== VERSION) throw new Error('invalid NebulaIM packet version');
    const bodyLength = view.getUint32(12, false);
    if (data.byteLength < HEADER_LENGTH + bodyLength) throw new Error('incomplete packet body');
    return {
      type: view.getUint16(6, false),
      sequenceId: view.getUint32(8, false),
      body: data.slice(HEADER_LENGTH, HEADER_LENGTH + bodyLength),
    };
  }
}

export class NebulaIMClient {
  constructor({ url, proto, heartbeatIntervalMs = 15000 }) {
    this.url = url;
    this.proto = proto;
    this.heartbeatIntervalMs = heartbeatIntervalMs;
    this.codec = new PacketCodec();
    this.socket = null;
    this.sequenceId = 1;
    this.pending = new Map();
    this.heartbeatTimer = null;
    this.userId = 0;
    this.token = '';
    this.onPush = null;
    this.onError = null;
  }

  connect() {
    return new Promise((resolve, reject) => {
      const ws = new WebSocket(this.url);
      ws.binaryType = 'arraybuffer';
      ws.onopen = () => {
        this.socket = ws;
        this.startHeartbeat();
        resolve();
      };
      ws.onerror = () => reject(new Error('NebulaIM WebSocket connection failed'));
      ws.onclose = () => this.stopHeartbeat();
      ws.onmessage = (event) => this.handleMessage(event.data);
    });
  }

  close() {
    this.stopHeartbeat();
    if (this.socket) this.socket.close();
    this.socket = null;
  }

  async register({ username, password, nickname }) {
    const req = this.proto.RegisterRequest.create({
      requestId: this.requestId('register'),
      username,
      password,
      nickname: nickname || username,
    });
    return this.request(MessageType.REGISTER_REQ, MessageType.REGISTER_RESP, this.proto.RegisterRequest.encode(req).finish(), this.proto.RegisterResponse.decode);
  }

  async login({ username, password, deviceId, platform = 'web', deviceName = '' }) {
    const req = this.proto.LoginRequest.create({
      requestId: this.requestId('login'),
      username,
      password,
      deviceId: deviceId || this.defaultDeviceId(),
      platform,
      deviceName,
    });
    const resp = await this.request(MessageType.LOGIN_REQ, MessageType.LOGIN_RESP, this.proto.LoginRequest.encode(req).finish(), this.proto.LoginResponse.decode);
    if (resp.response?.code === 0) {
      this.userId = Number(resp.userId || 0);
      this.token = resp.token || '';
    }
    return resp;
  }

  async sendSingleMessage({ toUserId, content, contentType = 1, clientSequenceId = 0 }) {
    const req = this.proto.SendSingleMessageRequest.create({
      requestId: this.requestId('send-single'),
      fromUserId: this.userId,
      toUserId,
      contentType,
      content,
      clientSequenceId,
    });
    return this.request(
      MessageType.SEND_SINGLE_MSG_REQ,
      MessageType.SEND_SINGLE_MSG_RESP,
      this.proto.SendSingleMessageRequest.encode(req).finish(),
      this.proto.SendSingleMessageResponse.decode,
    );
  }

  async ack(messageId) {
    const req = this.proto.AckMessageRequest.create({
      requestId: this.requestId('ack'),
      userId: this.userId,
      messageId,
    });
    return this.request(MessageType.ACK_REQ, MessageType.ACK_RESP, this.proto.AckMessageRequest.encode(req).finish(), this.proto.AckMessageResponse.decode);
  }

  async pullOfflineMessages(pageSize = 50) {
    const req = this.proto.PullOfflineMessagesRequest.create({
      requestId: this.requestId('pull-offline'),
      userId: this.userId,
      page: { page: 1, pageSize },
    });
    return this.request(
      MessageType.PULL_OFFLINE_MSG_REQ,
      MessageType.PULL_OFFLINE_MSG_RESP,
      this.proto.PullOfflineMessagesRequest.encode(req).finish(),
      this.proto.PullOfflineMessagesResponse.decode,
    );
  }

  request(type, responseType, body, decoder) {
    if (!this.socket || this.socket.readyState !== WebSocket.OPEN) {
      return Promise.reject(new Error('NebulaIM WebSocket is not connected'));
    }
    const sequenceId = this.sequenceId++;
    const payload = this.codec.encode(type, sequenceId, body);
    this.socket.send(payload);
    return new Promise((resolve, reject) => {
      this.pending.set(sequenceId, { responseType, decoder, resolve, reject });
      setTimeout(() => {
        if (!this.pending.has(sequenceId)) return;
        this.pending.delete(sequenceId);
        reject(new Error(`NebulaIM request timeout: ${type}`));
      }, 5000);
    });
  }

  handleMessage(data) {
    try {
      const packet = this.codec.decode(data);
      if (packet.type === MessageType.PUSH_MSG) {
        const message = this.proto.MessageData.decode(packet.body);
        if (this.onPush) this.onPush(message);
        return;
      }
      const pending = this.pending.get(packet.sequenceId);
      if (!pending) return;
      this.pending.delete(packet.sequenceId);
      if (packet.type !== pending.responseType && packet.type !== MessageType.ERROR_RESP) {
        pending.reject(new Error(`unexpected NebulaIM response type: ${packet.type}`));
        return;
      }
      pending.resolve(pending.decoder(packet.body));
    } catch (error) {
      if (this.onError) this.onError(error);
    }
  }

  startHeartbeat() {
    this.stopHeartbeat();
    this.heartbeatTimer = setInterval(() => {
      if (!this.socket || this.socket.readyState !== WebSocket.OPEN) return;
      this.socket.send(this.codec.encode(MessageType.HEARTBEAT_REQ, this.sequenceId++, new Uint8Array()));
    }, this.heartbeatIntervalMs);
  }

  stopHeartbeat() {
    if (this.heartbeatTimer) clearInterval(this.heartbeatTimer);
    this.heartbeatTimer = null;
  }

  requestId(prefix) {
    return `${prefix}-${Date.now()}-${Math.random().toString(16).slice(2)}`;
  }

  defaultDeviceId() {
    const key = 'nebulaim.device_id';
    let id = localStorage.getItem(key);
    if (!id) {
      id = `web-${Date.now()}-${Math.random().toString(16).slice(2)}`;
      localStorage.setItem(key, id);
    }
    return id;
  }
}
