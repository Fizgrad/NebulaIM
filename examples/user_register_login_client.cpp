#include "common/utils/TimeUtil.h"
#include "user.grpc.pb.h"

#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <string>

namespace {

std::string parseAddress(int argc, char** argv) {
    std::string address = "127.0.0.1:50051";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--addr" && i + 1 < argc) {
            address = argv[++i];
        }
    }
    return address;
}

std::string tokenPrefix(const std::string& token) {
    return token.size() <= 8 ? token : token.substr(0, 8);
}

}  // namespace

int main(int argc, char** argv) {
    std::string address = parseAddress(argc, argv);
    auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    auto stub = nebula::proto::UserService::NewStub(channel);

    std::string username = "client_user_" + std::to_string(nebula::TimeUtil::nowMs());

    nebula::proto::RegisterRequest register_req;
    register_req.set_request_id("client-register");
    register_req.set_username(username);
    register_req.set_password("password123");
    register_req.set_nickname("Client User");
    nebula::proto::RegisterResponse register_resp;
    grpc::ClientContext register_ctx;
    grpc::Status register_status = stub->Register(&register_ctx, register_req, &register_resp);
    if (!register_status.ok()) {
        std::cerr << "Register RPC failed: " << register_status.error_message() << std::endl;
        return 1;
    }
    std::cout << "register code=" << register_resp.response().code() << " user_id=" << register_resp.user_id() << std::endl;

    nebula::proto::LoginRequest login_req;
    login_req.set_request_id("client-login");
    login_req.set_username(username);
    login_req.set_password("password123");
    nebula::proto::LoginResponse login_resp;
    grpc::ClientContext login_ctx;
    grpc::Status login_status = stub->Login(&login_ctx, login_req, &login_resp);
    if (!login_status.ok() || login_resp.response().code() != 0) {
        std::cerr << "Login failed code=" << login_resp.response().code() << std::endl;
        return 1;
    }
    std::cout << "login user_id=" << login_resp.user_id() << " token_prefix=" << tokenPrefix(login_resp.token()) << std::endl;

    nebula::proto::ValidateTokenRequest validate_req;
    validate_req.set_request_id("client-validate");
    validate_req.set_token(login_resp.token());
    nebula::proto::ValidateTokenResponse validate_resp;
    grpc::ClientContext validate_ctx;
    grpc::Status validate_status = stub->ValidateToken(&validate_ctx, validate_req, &validate_resp);
    if (!validate_status.ok()) {
        std::cerr << "ValidateToken RPC failed" << std::endl;
        return 1;
    }
    std::cout << "validate valid=" << validate_resp.valid() << " user_id=" << validate_resp.user_id() << std::endl;

    nebula::proto::GetUserInfoRequest info_req;
    info_req.set_request_id("client-info");
    info_req.set_user_id(login_resp.user_id());
    nebula::proto::GetUserInfoResponse info_resp;
    grpc::ClientContext info_ctx;
    grpc::Status info_status = stub->GetUserInfo(&info_ctx, info_req, &info_resp);
    if (!info_status.ok()) {
        std::cerr << "GetUserInfo RPC failed" << std::endl;
        return 1;
    }
    std::cout << "user username=" << info_resp.user().username() << " nickname=" << info_resp.user().nickname() << std::endl;

    return 0;
}
