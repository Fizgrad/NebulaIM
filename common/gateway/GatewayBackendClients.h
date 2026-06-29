#pragma once

#include "message.grpc.pb.h"
#include "push.grpc.pb.h"
#include "relation.grpc.pb.h"
#include "common/rpc/GrpcTlsCredentials.h"
#include "user.grpc.pb.h"

#include <memory>
#include <string>

namespace nebula {

class ServiceResolver;

class GatewayBackendClients {
public:
    GatewayBackendClients() = default;

    bool init(const std::string& user_service_addr,
              const std::string& message_service_addr,
              const std::string& relation_service_addr,
              const std::string& push_service_addr);
    bool init(const std::string& user_service_addr,
              const std::string& message_service_addr,
              const std::string& relation_service_addr,
              const std::string& push_service_addr,
              const GrpcTlsConfig& tls_config);
    bool init(ServiceResolver& resolver);
    bool init(ServiceResolver& resolver, const GrpcTlsConfig& tls_config);

    proto::UserService::Stub* userService();
    proto::MessageService::Stub* messageService();
    proto::RelationService::Stub* relationService();
    proto::PushService::Stub* pushService();

private:
    std::unique_ptr<proto::UserService::Stub> user_stub_;
    std::unique_ptr<proto::MessageService::Stub> message_stub_;
    std::unique_ptr<proto::RelationService::Stub> relation_stub_;
    std::unique_ptr<proto::PushService::Stub> push_stub_;
};

}  // namespace nebula
