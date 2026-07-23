#include "UserServiceContext.h"
#include "UserServiceImpl.h"
#include "common/utils/TimeUtil.h"

#include <cassert>
#include <grpcpp/grpcpp.h>
#include <iostream>

int main() {
    nebula::UserServiceContext context;
    if (!context.init("config/nebula.conf")) {
        std::cout << "test_grpc_user_service skipped: MySQL/Redis dependencies are not reachable\n";
        return 0;
    }
    nebula::UserServiceImpl service(context.userDao(), context.deviceDao(), context.redisClient(), context.tokenManager(), context.passwordMinLength());
    grpc::ServerContext server_context;

    std::string username = "grpc_user_" + std::to_string(nebula::TimeUtil::nowMs());
    nebula::proto::RegisterRequest register_req;
    register_req.set_request_id("grpc-register");
    register_req.set_username(username);
    register_req.set_password("password123");
    register_req.set_nickname("Grpc User");
    nebula::proto::RegisterResponse register_resp;
    assert(service.Register(&server_context, &register_req, &register_resp).ok());
    assert(register_resp.response().code() == 0);

    nebula::proto::LoginRequest request;
    request.set_request_id("test-login");
    request.set_username(username);
    request.set_password("password123");
    request.set_device_id("grpc-device");
    request.set_platform("test");
    request.set_device_name("grpc-device");

    nebula::proto::LoginResponse response;
    grpc::Status status = service.Login(&server_context, &request, &response);

    assert(status.ok());
    assert(response.response().code() == 0);
    assert(response.response().message() == "OK");
    assert(response.response().request_id() == "test-login");
    assert(response.user_id() == register_resp.user_id());
    assert(!response.token().empty());

    return 0;
}
