#pragma once

#include "common/net/Buffer.h"
#include "common/protocol/PacketCodec.h"
#include "message.pb.h"
#include "user.pb.h"

#include <cstdint>
#include <deque>
#include <string>

namespace google::protobuf {
class MessageLite;
}

namespace nebula::benchmark {

struct LoginResult {
    uint64_t user_id = 0;
    std::string token;
    int64_t expire_at = 0;
};

class GatewayBenchClient {
public:
    GatewayBenchClient(std::string host, uint16_t port);
    ~GatewayBenchClient();

    GatewayBenchClient(const GatewayBenchClient&) = delete;
    GatewayBenchClient& operator=(const GatewayBenchClient&) = delete;

    bool connectGateway(std::string* error);
    bool registerUser(const std::string& username, const std::string& password, uint64_t* user_id, std::string* error);
    bool loginUser(const std::string& username, const std::string& password, const std::string& device_id, LoginResult* result, std::string* error);
    bool sendSingleMessage(uint64_t from_user_id, uint64_t to_user_id, const std::string& content, uint32_t client_sequence_id, uint64_t* message_id, std::string* error);
    bool sendGroupMessage(uint64_t from_user_id, uint64_t group_id, const std::string& content, uint32_t client_sequence_id, uint64_t* message_id, std::string* error);
    bool ackMessage(uint64_t user_id, uint64_t message_id, std::string* error);
    bool waitPush(proto::MessageData* message, int timeout_ms, std::string* error);

private:
    uint32_t nextSequence();
    bool sendProto(MessageType type, uint32_t sequence_id, const google::protobuf::MessageLite& msg, std::string* error);
    bool waitForPacket(MessageType type, uint32_t sequence_id, Packet* packet, int timeout_ms, std::string* error);
    bool readNextPacket(Packet* packet, int timeout_ms, std::string* error);
    bool readFromSocket(int timeout_ms, std::string* error);
    bool writeAll(const std::string& data, std::string* error);
    bool waitReadable(int timeout_ms) const;
    void closeSocket();

private:
    std::string host_;
    uint16_t port_ = 0;
    int fd_ = -1;
    uint32_t sequence_ = 1;
    Buffer buffer_;
    PacketCodec packet_codec_;
    std::deque<Packet> pending_;
};

}  // namespace nebula::benchmark
