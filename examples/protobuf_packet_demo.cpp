#include "common/net/Buffer.h"
#include "common/protocol/PacketCodec.h"
#include "message.pb.h"

#include <cassert>
#include <iostream>

int main() {
    nebula::proto::SendSingleMessageRequest request;
    request.set_request_id("packet-demo-1");
    request.set_from_user_id(10001);
    request.set_to_user_id(10002);
    request.set_content_type(nebula::proto::MESSAGE_CONTENT_TYPE_TEXT);
    request.set_content("hello protobuf packet");
    request.set_client_sequence_id(7);

    std::string body;
    assert(request.SerializeToString(&body));

    nebula::Packet packet(nebula::MessageType::SEND_SINGLE_MSG_REQ, request.client_sequence_id(), body);
    nebula::PacketCodec codec;
    std::string encoded = codec.encode(packet);

    nebula::Buffer buffer;
    buffer.append(encoded);

    nebula::Packet decoded_packet;
    assert(codec.decode(&buffer, &decoded_packet) == nebula::ProtocolError::OK);
    assert(decoded_packet.sequence_id == 7);

    nebula::proto::SendSingleMessageRequest decoded_request;
    assert(decoded_request.ParseFromString(decoded_packet.body));
    assert(decoded_request.request_id() == request.request_id());
    assert(decoded_request.from_user_id() == request.from_user_id());
    assert(decoded_request.to_user_id() == request.to_user_id());
    assert(decoded_request.content() == request.content());
    assert(decoded_request.client_sequence_id() == request.client_sequence_id());

    std::cout << "protobuf packet demo ok" << std::endl;
    return 0;
}
