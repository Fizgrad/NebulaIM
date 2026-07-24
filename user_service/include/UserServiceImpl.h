#pragma once

#include "user.grpc.pb.h"

namespace nebula {

class RedisClient;
class TokenManager;
class DeviceDao;
class UserDao;

class UserServiceImpl final : public proto::UserService::Service {
public:
    UserServiceImpl(UserDao* user_dao,
                    DeviceDao* device_dao,
                    RedisClient* redis_client,
                    TokenManager* token_manager,
                    int password_min_length);

    grpc::Status Register(grpc::ServerContext* context,
                          const proto::RegisterRequest* request,
                          proto::RegisterResponse* response) override;

    grpc::Status Login(grpc::ServerContext* context,
                       const proto::LoginRequest* request,
                       proto::LoginResponse* response) override;

    grpc::Status ValidateToken(grpc::ServerContext* context,
                               const proto::ValidateTokenRequest* request,
                               proto::ValidateTokenResponse* response) override;

    grpc::Status GetUserInfo(grpc::ServerContext* context,
                             const proto::GetUserInfoRequest* request,
                             proto::GetUserInfoResponse* response) override;

    grpc::Status GetUserByUsername(grpc::ServerContext* context,
                                   const proto::GetUserByUsernameRequest* request,
                                   proto::GetUserInfoResponse* response) override;

    grpc::Status RefreshToken(grpc::ServerContext* context,
                              const proto::RefreshTokenRequest* request,
                              proto::RefreshTokenResponse* response) override;

private:
    UserDao* user_dao_;
    DeviceDao* device_dao_;
    RedisClient* redis_client_;
    TokenManager* token_manager_;
    int password_min_length_;
};

}  // namespace nebula
