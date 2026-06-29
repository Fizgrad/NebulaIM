#include "common/gateway/GatewayBackendClients.h"

#include <grpcpp/grpcpp.h>

namespace nebula {

bool GatewayBackendClients::init(const std::string& user_service_addr,
                                 const std::string& message_service_addr,
                                 const std::string& relation_service_addr,
                                 const std::string& push_service_addr) {
    user_stub_ = proto::UserService::NewStub(grpc::CreateChannel(user_service_addr, grpc::InsecureChannelCredentials()));
    message_stub_ = proto::MessageService::NewStub(grpc::CreateChannel(message_service_addr, grpc::InsecureChannelCredentials()));
    relation_stub_ = proto::RelationService::NewStub(grpc::CreateChannel(relation_service_addr, grpc::InsecureChannelCredentials()));
    push_stub_ = proto::PushService::NewStub(grpc::CreateChannel(push_service_addr, grpc::InsecureChannelCredentials()));
    return user_stub_ && message_stub_ && relation_stub_ && push_stub_;
}

proto::UserService::Stub* GatewayBackendClients::userService() { return user_stub_.get(); }
proto::MessageService::Stub* GatewayBackendClients::messageService() { return message_stub_.get(); }
proto::RelationService::Stub* GatewayBackendClients::relationService() { return relation_stub_.get(); }
proto::PushService::Stub* GatewayBackendClients::pushService() { return push_stub_.get(); }

}  // namespace nebula
