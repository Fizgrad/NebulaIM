#include "GatewayBenchClient.h"

#include "common/protocol/ProtocolError.h"
#include "common/websocket/WebSocketFrame.h"
#include "google/protobuf/message_lite.h"

#include <arpa/inet.h>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <exception>
#include <netdb.h>
#include <sstream>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace nebula::benchmark {

namespace {

constexpr int kDefaultTimeoutMs = 5000;

void setError(std::string* error, const std::string& message) {
    if (error != nullptr) *error = message;
}

bool responseOk(const proto::CommonResponse& response, std::string* error) {
    if (response.code() == 0) return true;
    setError(error, "response code=" + std::to_string(response.code()) + " message=" + response.message());
    return false;
}

bool parseCommonError(const Packet& packet, std::string* error) {
    if (packet.type != MessageType::ERROR_RESP) return false;
    proto::CommonResponse response;
    if (response.ParseFromString(packet.body)) {
        setError(error, "gateway error code=" + std::to_string(response.code()) + " message=" + response.message());
    } else {
        setError(error, "gateway error response parse failed");
    }
    return true;
}

}  // namespace

GatewayBenchClient::GatewayBenchClient(std::string host, uint16_t port) : host_(std::move(host)), port_(port) {}

GatewayBenchClient::~GatewayBenchClient() {
    closeSocket();
}

bool GatewayBenchClient::connectGateway(std::string* error) {
    closeSocket();
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        setError(error, "socket failed: " + std::string(std::strerror(errno)));
        return false;
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    int rc = ::getaddrinfo(host_.c_str(), std::to_string(port_).c_str(), &hints, &result);
    if (rc != 0 || result == nullptr) {
        setError(error, "getaddrinfo failed: " + std::string(::gai_strerror(rc)));
        closeSocket();
        return false;
    }
    bool connected = false;
    for (addrinfo* item = result; item != nullptr; item = item->ai_next) {
        if (::connect(fd_, item->ai_addr, item->ai_addrlen) == 0) {
            connected = true;
            break;
        }
    }
    ::freeaddrinfo(result);
    if (!connected) {
        setError(error, "connect failed: " + std::string(std::strerror(errno)));
        closeSocket();
        return false;
    }

    std::ostringstream request;
    request << "GET /ws HTTP/1.1\r\n"
            << "Host: " << host_ << ":" << port_ << "\r\n"
            << "Upgrade: websocket\r\n"
            << "Connection: Upgrade\r\n"
            << "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"  // gitleaks:allow RFC 6455 example nonce
            << "Sec-WebSocket-Version: 13\r\n"
            << "Origin: http://" << host_ << "\r\n\r\n";
    if (!writeAll(request.str(), error)) {
        closeSocket();
        return false;
    }

    std::string response;
    char buf[1024];
    while (response.find("\r\n\r\n") == std::string::npos) {
        if (!waitReadable(kDefaultTimeoutMs)) {
            setError(error, "websocket handshake timed out");
            closeSocket();
            return false;
        }
        ssize_t n = ::recv(fd_, buf, sizeof(buf), 0);
        if (n <= 0) {
            setError(error, "websocket handshake read failed");
            closeSocket();
            return false;
        }
        response.append(buf, static_cast<size_t>(n));
        if (response.size() > 8192) {
            setError(error, "websocket handshake response too large");
            closeSocket();
            return false;
        }
    }
    if (response.find("101 Switching Protocols") == std::string::npos) {
        setError(error, "websocket upgrade rejected");
        closeSocket();
        return false;
    }
    return true;
}

bool GatewayBenchClient::registerUser(const std::string& username, const std::string& password, uint64_t* user_id, std::string* error) {
    proto::RegisterRequest request;
    request.set_request_id("bench-register-" + username);
    request.set_username(username);
    request.set_password(password);
    request.set_nickname(username);
    uint32_t sequence = nextSequence();
    if (!sendProto(MessageType::REGISTER_REQ, sequence, request, error)) return false;
    Packet packet;
    if (!waitForPacket(MessageType::REGISTER_RESP, sequence, &packet, kDefaultTimeoutMs, error)) return false;
    proto::RegisterResponse response;
    if (!response.ParseFromString(packet.body)) {
        setError(error, "register response parse failed");
        return false;
    }
    if (!responseOk(response.response(), error)) return false;
    if (user_id != nullptr) *user_id = response.user_id();
    return response.user_id() != 0;
}

bool GatewayBenchClient::loginUser(const std::string& username, const std::string& password, const std::string& device_id, LoginResult* result, std::string* error) {
    proto::LoginRequest request;
    request.set_request_id("bench-login-" + username);
    request.set_username(username);
    request.set_password(password);
    request.set_device_id(device_id);
    request.set_platform("benchmark");
    request.set_device_name(device_id);
    uint32_t sequence = nextSequence();
    if (!sendProto(MessageType::LOGIN_REQ, sequence, request, error)) return false;
    Packet packet;
    if (!waitForPacket(MessageType::LOGIN_RESP, sequence, &packet, kDefaultTimeoutMs, error)) return false;
    proto::LoginResponse response;
    if (!response.ParseFromString(packet.body)) {
        setError(error, "login response parse failed");
        return false;
    }
    if (!responseOk(response.response(), error)) return false;
    if (result != nullptr) {
        result->user_id = response.user_id();
        result->token = response.token();
        result->expire_at = response.expire_at();
    }
    return response.user_id() != 0 && !response.token().empty();
}

bool GatewayBenchClient::sendSingleMessage(uint64_t from_user_id,
                                           uint64_t to_user_id,
                                           const std::string& content,
                                           uint32_t client_sequence_id,
                                           uint64_t* message_id,
                                           std::string* error) {
    proto::SendSingleMessageRequest request;
    request.set_request_id("bench-send-single-" + std::to_string(client_sequence_id));
    request.set_from_user_id(from_user_id);
    request.set_to_user_id(to_user_id);
    request.set_content_type(proto::MESSAGE_CONTENT_TYPE_TEXT);
    request.set_content(content);
    request.set_client_sequence_id(client_sequence_id);
    uint32_t sequence = nextSequence();
    if (!sendProto(MessageType::SEND_SINGLE_MSG_REQ, sequence, request, error)) return false;
    Packet packet;
    if (!waitForPacket(MessageType::SEND_SINGLE_MSG_RESP, sequence, &packet, kDefaultTimeoutMs, error)) return false;
    proto::SendSingleMessageResponse response;
    if (!response.ParseFromString(packet.body)) {
        setError(error, "single-message response parse failed");
        return false;
    }
    if (!responseOk(response.response(), error)) return false;
    if (message_id != nullptr) *message_id = response.message_id();
    return response.message_id() != 0;
}

bool GatewayBenchClient::sendGroupMessage(uint64_t from_user_id,
                                          uint64_t group_id,
                                          const std::string& content,
                                          uint32_t client_sequence_id,
                                          uint64_t* message_id,
                                          std::string* error) {
    proto::SendGroupMessageRequest request;
    request.set_request_id("bench-send-group-" + std::to_string(client_sequence_id));
    request.set_from_user_id(from_user_id);
    request.set_group_id(group_id);
    request.set_content_type(proto::MESSAGE_CONTENT_TYPE_TEXT);
    request.set_content(content);
    request.set_client_sequence_id(client_sequence_id);
    uint32_t sequence = nextSequence();
    if (!sendProto(MessageType::SEND_GROUP_MSG_REQ, sequence, request, error)) return false;
    Packet packet;
    if (!waitForPacket(MessageType::SEND_GROUP_MSG_RESP, sequence, &packet, kDefaultTimeoutMs, error)) return false;
    proto::SendGroupMessageResponse response;
    if (!response.ParseFromString(packet.body)) {
        setError(error, "group-message response parse failed");
        return false;
    }
    if (!responseOk(response.response(), error)) return false;
    if (message_id != nullptr) *message_id = response.message_id();
    return response.message_id() != 0;
}

bool GatewayBenchClient::ackMessage(uint64_t user_id, uint64_t message_id, std::string* error) {
    proto::AckMessageRequest request;
    request.set_request_id("bench-ack-" + std::to_string(message_id));
    request.set_user_id(user_id);
    request.set_message_id(message_id);
    uint32_t sequence = nextSequence();
    if (!sendProto(MessageType::ACK_REQ, sequence, request, error)) return false;
    Packet packet;
    if (!waitForPacket(MessageType::ACK_RESP, sequence, &packet, kDefaultTimeoutMs, error)) return false;
    proto::AckMessageResponse response;
    if (!response.ParseFromString(packet.body)) {
        setError(error, "ack response parse failed");
        return false;
    }
    return responseOk(response.response(), error);
}

bool GatewayBenchClient::waitPush(proto::MessageData* message, int timeout_ms, std::string* error) {
    Packet packet;
    if (!waitForPacket(MessageType::PUSH_MSG, 0, &packet, timeout_ms, error)) return false;
    if (message == nullptr || !message->ParseFromString(packet.body)) {
        setError(error, "push message parse failed");
        return false;
    }
    return true;
}

uint32_t GatewayBenchClient::nextSequence() {
    return sequence_++;
}

bool GatewayBenchClient::sendProto(MessageType type, uint32_t sequence_id, const google::protobuf::MessageLite& msg, std::string* error) {
    std::string body;
    if (!msg.SerializeToString(&body)) {
        setError(error, "protobuf serialize failed");
        return false;
    }
    std::string packet;
    try {
        packet = packet_codec_.encode(Packet(type, sequence_id, std::move(body)));
    } catch (const std::exception& ex) {
        setError(error, ex.what());
        return false;
    }
    return writeAll(WebSocketFrameCodec::encode(WebSocketOpCode::BINARY, packet, true), error);
}

bool GatewayBenchClient::waitForPacket(MessageType type, uint32_t sequence_id, Packet* packet, int timeout_ms, std::string* error) {
    for (auto it = pending_.begin(); it != pending_.end(); ++it) {
        if (it->type == type && (sequence_id == 0 || it->sequence_id == sequence_id)) {
            if (packet != nullptr) *packet = std::move(*it);
            pending_.erase(it);
            return true;
        }
    }

    const auto start = std::chrono::steady_clock::now();
    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        int remaining = timeout_ms - static_cast<int>(elapsed);
        if (remaining <= 0) {
            setError(error, "packet wait timed out");
            return false;
        }
        Packet next;
        if (!readNextPacket(&next, remaining, error)) return false;
        if (parseCommonError(next, error)) return false;
        if (next.type == type && (sequence_id == 0 || next.sequence_id == sequence_id)) {
            if (packet != nullptr) *packet = std::move(next);
            return true;
        }
        pending_.push_back(std::move(next));
    }
}

bool GatewayBenchClient::readNextPacket(Packet* packet, int timeout_ms, std::string* error) {
    const auto start = std::chrono::steady_clock::now();
    while (true) {
        while (buffer_.readableBytes() > 0) {
            WebSocketFrame frame;
            size_t consumed = 0;
            auto status = WebSocketFrameCodec::decode(buffer_.peek(), buffer_.readableBytes(), false, &frame, &consumed);
            if (status == WebSocketFrameStatus::INCOMPLETE) break;
            if (status != WebSocketFrameStatus::OK) {
                setError(error, "websocket frame decode failed");
                return false;
            }
            buffer_.retrieve(consumed);
            if (frame.opcode == WebSocketOpCode::PING) {
                if (!writeAll(WebSocketFrameCodec::encode(WebSocketOpCode::PONG, frame.payload, true), error)) return false;
                continue;
            }
            if (frame.opcode == WebSocketOpCode::PONG) continue;
            if (frame.opcode != WebSocketOpCode::BINARY) {
                setError(error, "unexpected websocket opcode");
                return false;
            }
            Buffer packet_buffer;
            packet_buffer.append(frame.payload);
            if (packet_codec_.decode(&packet_buffer, packet) != ProtocolError::OK) {
                setError(error, "packet decode failed");
                return false;
            }
            return true;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        int remaining = timeout_ms - static_cast<int>(elapsed);
        if (remaining <= 0) {
            setError(error, "socket read timed out");
            return false;
        }
        if (!readFromSocket(remaining, error)) return false;
    }
}

bool GatewayBenchClient::readFromSocket(int timeout_ms, std::string* error) {
    if (!waitReadable(timeout_ms)) {
        setError(error, "socket read timed out");
        return false;
    }
    char buf[8192];
    ssize_t n = ::recv(fd_, buf, sizeof(buf), 0);
    if (n <= 0) {
        setError(error, "socket read failed");
        return false;
    }
    buffer_.append(buf, static_cast<size_t>(n));
    return true;
}

bool GatewayBenchClient::writeAll(const std::string& data, std::string* error) {
    size_t written = 0;
    while (written < data.size()) {
        ssize_t n = ::send(fd_, data.data() + written, data.size() - written, MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) {
            setError(error, "socket write failed: " + std::string(std::strerror(errno)));
            return false;
        }
        written += static_cast<size_t>(n);
    }
    return true;
}

bool GatewayBenchClient::waitReadable(int timeout_ms) const {
    if (fd_ < 0) return false;
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd_, &rfds);
    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    int rc = ::select(fd_ + 1, &rfds, nullptr, nullptr, &tv);
    return rc > 0 && FD_ISSET(fd_, &rfds);
}

void GatewayBenchClient::closeSocket() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    buffer_.retrieveAll();
    pending_.clear();
}

}  // namespace nebula::benchmark
