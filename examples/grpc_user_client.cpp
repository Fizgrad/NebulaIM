#include "common/log/Logger.h"
#include "common/rpc/GrpcClient.h"
#include "user.grpc.pb.h"

#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>

int main() {
    auto channel = nebula::GrpcClient::createInsecureChannel("127.0.0.1:50051");
    auto stub = nebula::proto::UserService::NewStub(channel);

    nebula::proto::LoginRequest login_req;
    login_req.set_request_id("example-login-1");
    login_req.set_username("mock_user");
    login_req.set_password("mock_password");

    nebula::proto::LoginResponse login_resp;
    grpc::ClientContext login_ctx;
    grpc::Status login_status = stub->Login(&login_ctx, login_req, &login_resp);
    if (!login_status.ok()) {
        std::cerr << "Login RPC failed: " << login_status.error_message() << std::endl;
        return 1;
    }

    std::cout << "login user_id=" << login_resp.user_id()
              << " token=" << login_resp.token()
              << " expire_at=" << login_resp.expire_at() << std::endl;

    nebula::proto::ValidateTokenRequest validate_req;
    validate_req.set_request_id("example-validate-1");
    validate_req.set_token(login_resp.token());

    nebula::proto::ValidateTokenResponse validate_resp;
    grpc::ClientContext validate_ctx;
    grpc::Status validate_status = stub->ValidateToken(&validate_ctx, validate_req, &validate_resp);
    if (!validate_status.ok()) {
        std::cerr << "ValidateToken RPC failed: " << validate_status.error_message() << std::endl;
        return 1;
    }

    std::cout << "validate valid=" << validate_resp.valid()
              << " user_id=" << validate_resp.user_id() << std::endl;
    return 0;
}
