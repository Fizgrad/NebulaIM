#include "common/gateway/GatewayBackendClients.h"

#include "common/discovery/ServiceDiscovery.h"
#include "common/rpc/GrpcClient.h"

#include <grpcpp/grpcpp.h>

namespace nebula {

bool GatewayBackendClients::init(const std::string& user_service_addr,
                                 const std::string& message_service_addr,
                                 const std::string& relation_service_addr,
                                 const std::string& push_service_addr) {
    return init(user_service_addr, message_service_addr, relation_service_addr, push_service_addr, GrpcTlsConfig{});
}

bool GatewayBackendClients::init(const std::string& user_service_addr,
                                 const std::string& message_service_addr,
                                 const std::string& relation_service_addr,
                                 const std::string& push_service_addr,
                                 const GrpcTlsConfig& tls_config) {
    auto user_channel = GrpcClient::createChannel(user_service_addr, tls_config);
    auto message_channel = GrpcClient::createChannel(message_service_addr, tls_config);
    auto relation_channel = GrpcClient::createChannel(relation_service_addr, tls_config);
    auto push_channel = GrpcClient::createChannel(push_service_addr, tls_config);
    if (!user_channel || !message_channel || !relation_channel || !push_channel) {
        return false;
    }
    user_stub_ = proto::UserService::NewStub(user_channel);
    message_stub_ = proto::MessageService::NewStub(message_channel);
    relation_stub_ = proto::RelationService::NewStub(relation_channel);
    push_stub_ = proto::PushService::NewStub(push_channel);
    return user_stub_ && message_stub_ && relation_stub_ && push_stub_;
}

bool GatewayBackendClients::init(ServiceResolver& resolver) {
    return init(resolver, GrpcTlsConfig{});
}

bool GatewayBackendClients::init(ServiceResolver& resolver, const GrpcTlsConfig& tls_config) {
    try {
        return init(resolver.pick("user_service").address,
                    resolver.pick("message_service").address,
                    resolver.pick("relation_service").address,
                    resolver.pick("push_service").address,
                    tls_config);
    } catch (...) {
        return false;
    }
}

proto::UserService::Stub* GatewayBackendClients::userService() { return user_stub_.get(); }
proto::MessageService::Stub* GatewayBackendClients::messageService() { return message_stub_.get(); }
proto::RelationService::Stub* GatewayBackendClients::relationService() { return relation_stub_.get(); }
proto::PushService::Stub* GatewayBackendClients::pushService() { return push_stub_.get(); }

}  // namespace nebula
