#include "common/net/Buffer.h"
#include "common/protocol/PacketCodec.h"
#include "common/protocol/ProtocolError.h"
#include "common/utils/TimeUtil.h"
#include "common/websocket/WebSocketFrame.h"
#include "conversation.grpc.pb.h"
#include "message.grpc.pb.h"
#include "relation.grpc.pb.h"
#include "user.pb.h"

#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <grpcpp/grpcpp.h>

namespace {

constexpr int kGatewayPort = 9000;
constexpr int kUserPort = 50051;
constexpr int kMessagePort = 50052;
constexpr int kRelationPort = 50053;
constexpr int kConversationPort = 50056;

bool shouldRunFullE2E() {
    const char* value = std::getenv("NEBULA_RUN_BACKEND_E2E");
    return value != nullptr && std::string(value) == "1";
}

bool canConnect(uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    bool ok = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
    ::close(fd);
    return ok;
}

void requireServices() {
    const uint16_t ports[] = {kUserPort, kMessagePort, kRelationPort, kConversationPort, kGatewayPort};
    for (uint16_t port : ports) {
        if (!canConnect(port)) {
            std::cerr << "backend_final_e2e requires service on 127.0.0.1:" << port << "\n";
            std::exit(1);
        }
    }
}

bool writeAll(int fd, const std::string& data) {
    size_t written = 0;
    while (written < data.size()) {
        ssize_t n = ::write(fd, data.data() + written, data.size() - written);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        written += static_cast<size_t>(n);
    }
    return true;
}

bool waitReadable(int fd, int timeout_ms) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    int rc = ::select(fd + 1, &rfds, nullptr, nullptr, &tv);
    return rc > 0 && FD_ISSET(fd, &rfds);
}

class WsClient {
public:
    ~WsClient() {
        if (fd_ >= 0) ::close(fd_);
    }

    bool connectToGateway() {
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) return false;
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(kGatewayPort);
        ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        if (::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) return false;
        std::string request =
            "GET /ws HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "Origin: http://127.0.0.1\r\n\r\n";
        if (!writeAll(fd_, request)) return false;
        std::string response;
        char buf[1024];
        const int64_t deadline = nebula::TimeUtil::nowMs() + 3000;
        while (nebula::TimeUtil::nowMs() < deadline) {
            if (!waitReadable(fd_, 250)) continue;
            ssize_t n = ::read(fd_, buf, sizeof(buf));
            if (n <= 0) return false;
            response.append(buf, static_cast<size_t>(n));
            if (response.find("\r\n\r\n") != std::string::npos) break;
        }
        return response.find("101 Switching Protocols") != std::string::npos;
    }

    template <typename Proto>
    bool sendProto(nebula::MessageType type, uint32_t sequence_id, const Proto& msg) {
        std::string body;
        msg.SerializeToString(&body);
        std::string packet = packet_codec_.encode(nebula::Packet(type, sequence_id, body));
        return writeAll(fd_, nebula::WebSocketFrameCodec::encode(nebula::WebSocketOpCode::BINARY, packet, true));
    }

    bool readPacket(nebula::Packet* packet, int timeout_ms) {
        const int64_t deadline = nebula::TimeUtil::nowMs() + timeout_ms;
        while (nebula::TimeUtil::nowMs() < deadline) {
            while (buffer_.readableBytes() > 0) {
                nebula::WebSocketFrame frame;
                size_t consumed = 0;
                auto status = nebula::WebSocketFrameCodec::decode(buffer_.peek(), buffer_.readableBytes(), false, &frame, &consumed);
                if (status == nebula::WebSocketFrameStatus::INCOMPLETE) break;
                if (status != nebula::WebSocketFrameStatus::OK) return false;
                buffer_.retrieve(consumed);
                if (frame.opcode == nebula::WebSocketOpCode::PING) {
                    writeAll(fd_, nebula::WebSocketFrameCodec::encode(nebula::WebSocketOpCode::PONG, frame.payload, true));
                    continue;
                }
                if (frame.opcode == nebula::WebSocketOpCode::PONG) continue;
                if (frame.opcode != nebula::WebSocketOpCode::BINARY) return false;
                nebula::Buffer packet_buffer;
                packet_buffer.append(frame.payload);
                return packet_codec_.decode(&packet_buffer, packet) == nebula::ProtocolError::OK;
            }

            int remaining = static_cast<int>(deadline - nebula::TimeUtil::nowMs());
            if (remaining <= 0 || !waitReadable(fd_, remaining > 250 ? 250 : remaining)) continue;
            char buf[8192];
            ssize_t n = ::read(fd_, buf, sizeof(buf));
            if (n <= 0) return false;
            buffer_.append(buf, static_cast<size_t>(n));
        }
        return false;
    }

private:
    int fd_ = -1;
    nebula::Buffer buffer_;
    nebula::PacketCodec packet_codec_;
};

template <typename Resp>
Resp expectResponse(WsClient* client, nebula::MessageType expected_type, uint32_t sequence_id) {
    nebula::Packet packet;
    assert(client->readPacket(&packet, 5000));
    assert(packet.type == expected_type);
    assert(packet.sequence_id == sequence_id);
    Resp resp;
    assert(resp.ParseFromString(packet.body));
    assert(resp.response().code() == 0);
    return resp;
}

nebula::proto::RegisterResponse gatewayRegister(WsClient* client, const std::string& username, uint32_t seq) {
    nebula::proto::RegisterRequest req;
    req.set_request_id("e2e-register-" + username);
    req.set_username(username);
    req.set_password("password123");
    req.set_nickname(username);
    assert(client->sendProto(nebula::MessageType::REGISTER_REQ, seq, req));
    return expectResponse<nebula::proto::RegisterResponse>(client, nebula::MessageType::REGISTER_RESP, seq);
}

nebula::proto::LoginResponse gatewayLogin(WsClient* client, const std::string& username, const std::string& device_id, uint32_t seq) {
    nebula::proto::LoginRequest req;
    req.set_request_id("e2e-login-" + username);
    req.set_username(username);
    req.set_password("password123");
    req.set_device_id(device_id);
    req.set_platform("web-e2e");
    req.set_device_name(device_id);
    assert(client->sendProto(nebula::MessageType::LOGIN_REQ, seq, req));
    return expectResponse<nebula::proto::LoginResponse>(client, nebula::MessageType::LOGIN_RESP, seq);
}

void sendAck(WsClient* client, uint64_t user_id, uint64_t message_id, uint32_t seq) {
    nebula::proto::AckMessageRequest req;
    req.set_request_id("e2e-ack");
    req.set_user_id(user_id);
    req.set_message_id(message_id);
    assert(client->sendProto(nebula::MessageType::ACK_REQ, seq, req));
    (void)expectResponse<nebula::proto::AckMessageResponse>(client, nebula::MessageType::ACK_RESP, seq);
}

}  // namespace

int main() {
    if (!shouldRunFullE2E()) {
        std::cout << "test_backend_final_e2e skipped: set NEBULA_RUN_BACKEND_E2E=1 after starting services to run the full scenario.\n";
        return 0;
    }

    requireServices();

    auto channel = grpc::CreateChannel("127.0.0.1:50053", grpc::InsecureChannelCredentials());
    auto relation = nebula::proto::RelationService::NewStub(channel);
    auto message = nebula::proto::MessageService::NewStub(grpc::CreateChannel("127.0.0.1:50052", grpc::InsecureChannelCredentials()));
    auto conversation = nebula::proto::ConversationService::NewStub(grpc::CreateChannel("127.0.0.1:50056", grpc::InsecureChannelCredentials()));

    const std::string suffix = std::to_string(nebula::TimeUtil::nowMs()) + "_" + std::to_string(::getpid());
    const std::string user_a = "e2e_a_" + suffix;
    const std::string user_b = "e2e_b_" + suffix;

    WsClient a;
    WsClient b;
    assert(a.connectToGateway());
    assert(b.connectToGateway());

    auto reg_a = gatewayRegister(&a, user_a, 1);
    auto reg_b = gatewayRegister(&b, user_b, 1);
    assert(reg_a.user_id() != 0);
    assert(reg_b.user_id() != 0);

    auto login_a = gatewayLogin(&a, user_a, "web-a-" + suffix, 2);
    auto login_b = gatewayLogin(&b, user_b, "web-b-" + suffix, 2);
    assert(login_a.user_id() == reg_a.user_id());
    assert(login_b.user_id() == reg_b.user_id());

    nebula::proto::SendFriendRequestRequest friend_req;
    friend_req.set_request_id("e2e-friend-request");
    friend_req.set_from_user_id(reg_a.user_id());
    friend_req.set_to_user_id(reg_b.user_id());
    friend_req.set_message("hello");
    nebula::proto::SendFriendRequestResponse friend_resp;
    grpc::ClientContext friend_ctx;
    assert(relation->SendFriendRequest(&friend_ctx, friend_req, &friend_resp).ok());
    assert(friend_resp.response().code() == 0);
    assert(friend_resp.friend_request_id() != 0);

    nebula::proto::AcceptFriendRequestRequest accept_req;
    accept_req.set_request_id("e2e-friend-accept");
    accept_req.set_user_id(reg_b.user_id());
    accept_req.set_friend_request_id(friend_resp.friend_request_id());
    nebula::proto::CommonResponse accept_resp;
    grpc::ClientContext accept_ctx;
    assert(relation->AcceptFriendRequest(&accept_ctx, accept_req, &accept_resp).ok());
    assert(accept_resp.code() == 0);

    nebula::proto::SendSingleMessageRequest send_req;
    send_req.set_request_id("e2e-send-single");
    send_req.set_from_user_id(reg_a.user_id());
    send_req.set_to_user_id(reg_b.user_id());
    send_req.set_content_type(nebula::proto::MESSAGE_CONTENT_TYPE_TEXT);
    send_req.set_content("hello from backend final e2e");
    send_req.set_client_sequence_id(1001);
    assert(a.sendProto(nebula::MessageType::SEND_SINGLE_MSG_REQ, 3, send_req));
    auto send_resp = expectResponse<nebula::proto::SendSingleMessageResponse>(&a, nebula::MessageType::SEND_SINGLE_MSG_RESP, 3);
    assert(send_resp.message_id() != 0);

    nebula::Packet pushed_packet;
    assert(b.readPacket(&pushed_packet, 15000));
    assert(pushed_packet.type == nebula::MessageType::PUSH_MSG);
    nebula::proto::MessageData pushed;
    assert(pushed.ParseFromString(pushed_packet.body));
    assert(pushed.message_id() == send_resp.message_id());
    assert(pushed.from_user_id() == reg_a.user_id());
    assert(pushed.to_user_id() == reg_b.user_id());
    assert(!pushed.recalled());

    sendAck(&b, reg_b.user_id(), send_resp.message_id(), 3);

    nebula::proto::MarkMessageReadRequest read_req;
    read_req.set_request_id("e2e-mark-read");
    read_req.set_user_id(reg_b.user_id());
    read_req.set_message_id(send_resp.message_id());
    nebula::proto::CommonResponse read_resp;
    grpc::ClientContext read_ctx;
    assert(message->MarkMessageRead(&read_ctx, read_req, &read_resp).ok());
    assert(read_resp.code() == 0);

    nebula::proto::GetMessageReadStateRequest read_state_req;
    read_state_req.set_request_id("e2e-read-state");
    read_state_req.set_message_id(send_resp.message_id());
    nebula::proto::GetMessageReadStateResponse read_state_resp;
    grpc::ClientContext read_state_ctx;
    assert(message->GetMessageReadState(&read_state_ctx, read_state_req, &read_state_resp).ok());
    assert(read_state_resp.response().code() == 0);
    bool found_read = false;
    for (const auto& state : read_state_resp.states()) {
        if (state.user_id() == reg_b.user_id() && state.read_at() > 0) found_read = true;
    }
    assert(found_read);

    nebula::proto::RecallMessageRequest recall_req;
    recall_req.set_request_id("e2e-recall");
    recall_req.set_user_id(reg_a.user_id());
    recall_req.set_message_id(send_resp.message_id());
    nebula::proto::RecallMessageResponse recall_resp;
    grpc::ClientContext recall_ctx;
    assert(message->RecallMessage(&recall_ctx, recall_req, &recall_resp).ok());
    assert(recall_resp.response().code() == 0);
    assert(recall_resp.recalled_at() > 0);

    nebula::Packet recall_packet;
    assert(b.readPacket(&recall_packet, 15000));
    assert(recall_packet.type == nebula::MessageType::PUSH_MSG);
    nebula::proto::MessageData recall_data;
    assert(recall_data.ParseFromString(recall_packet.body));
    assert(recall_data.message_id() == send_resp.message_id());
    assert(recall_data.recalled());

    nebula::proto::ListConversationsRequest list_req;
    list_req.set_request_id("e2e-list-conversations");
    list_req.set_user_id(reg_b.user_id());
    list_req.mutable_page()->set_page(1);
    list_req.mutable_page()->set_page_size(20);
    nebula::proto::ListConversationsResponse list_resp;
    grpc::ClientContext list_ctx;
    assert(conversation->ListConversations(&list_ctx, list_req, &list_resp).ok());
    assert(list_resp.response().code() == 0);
    bool found_conversation = false;
    for (const auto& item : list_resp.conversations()) {
        if (item.conversation_id() == pushed.conversation_id()) {
            found_conversation = true;
            assert(item.last_message_id() == send_resp.message_id());
            assert(item.unread_count() == 0);
        }
    }
    assert(found_conversation);

    std::cout << "test_backend_final_e2e passed\n";
    return 0;
}
