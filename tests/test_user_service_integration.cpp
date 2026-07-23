#include "TestDeps.h"
#include "UserServiceContext.h"
#include "UserServiceImpl.h"
#include "common/utils/TimeUtil.h"

#include <cassert>
#include <grpcpp/grpcpp.h>

int main() {
    nebula::UserServiceContext context;
    if (!context.init("config/nebula.conf")) return nebula::tests::skip("test_user_service_integration", "UserService dependencies are not reachable");

    nebula::UserServiceImpl service(context.userDao(), context.deviceDao(), context.redisClient(), context.tokenManager(), context.passwordMinLength());
    grpc::ServerContext server_context;

    std::string username = "svc_integration_" + std::to_string(nebula::TimeUtil::nowMs());
    nebula::proto::RegisterRequest register_req;
    register_req.set_request_id("integration-register");
    register_req.set_username(username);
    register_req.set_password("password123");
    register_req.set_nickname("Integration User");
    nebula::proto::RegisterResponse register_resp;
    assert(service.Register(&server_context, &register_req, &register_resp).ok());
    assert(register_resp.response().code() == 0);

    nebula::proto::LoginRequest login_req;
    login_req.set_request_id("integration-login");
    login_req.set_username(username);
    login_req.set_password("password123");
    login_req.set_device_id("integration-device");
    login_req.set_platform("test");
    login_req.set_device_name("integration-device");
    nebula::proto::LoginResponse login_resp;
    assert(service.Login(&server_context, &login_req, &login_resp).ok());
    assert(login_resp.response().code() == 0);
    assert(!login_resp.token().empty());

    return 0;
}
