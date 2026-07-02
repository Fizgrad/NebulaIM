#include "UserServiceContext.h"
#include "UserServiceImpl.h"
#include "common/error/ErrorCode.h"
#include "common/utils/TimeUtil.h"

#include <cassert>
#include <grpcpp/grpcpp.h>

int main() {
    nebula::UserServiceContext context;
    assert(context.init("config/nebula.conf"));

    nebula::UserServiceImpl service(context.userDao(), context.redisClient(), context.tokenManager(), context.passwordMinLength());
    grpc::ServerContext server_context;

    std::string username = "svc_user_" + std::to_string(nebula::TimeUtil::nowMs());
    nebula::proto::RegisterRequest register_req;
    register_req.set_request_id("register-1");
    register_req.set_username(username);
    register_req.set_password("password123");
    register_req.set_nickname("Service User");
    nebula::proto::RegisterResponse register_resp;
    assert(service.Register(&server_context, &register_req, &register_resp).ok());
    assert(register_resp.response().code() == 0);
    assert(register_resp.user_id() > 0);

    nebula::proto::LoginRequest login_req;
    login_req.set_request_id("login-1");
    login_req.set_username(username);
    login_req.set_password("password123");
    nebula::proto::LoginResponse login_resp;
    assert(service.Login(&server_context, &login_req, &login_resp).ok());
    assert(login_resp.response().code() == 0);
    assert(login_resp.user_id() == register_resp.user_id());
    assert(!login_resp.token().empty());

    nebula::proto::ValidateTokenRequest validate_req;
    validate_req.set_request_id("validate-1");
    validate_req.set_token(login_resp.token());
    nebula::proto::ValidateTokenResponse validate_resp;
    assert(service.ValidateToken(&server_context, &validate_req, &validate_resp).ok());
    assert(validate_resp.response().code() == 0);
    assert(validate_resp.valid());
    assert(validate_resp.user_id() == register_resp.user_id());

    nebula::proto::GetUserInfoRequest info_req;
    info_req.set_request_id("info-1");
    info_req.set_user_id(register_resp.user_id());
    nebula::proto::GetUserInfoResponse info_resp;
    assert(service.GetUserInfo(&server_context, &info_req, &info_resp).ok());
    assert(info_resp.response().code() == 0);
    assert(info_resp.user().user_id() == register_resp.user_id());
    assert(info_resp.user().username() == username);

    nebula::proto::GetUserByUsernameRequest username_req;
    username_req.set_request_id("info-by-username-1");
    username_req.set_username(username);
    nebula::proto::GetUserInfoResponse username_resp;
    assert(service.GetUserByUsername(&server_context, &username_req, &username_resp).ok());
    assert(username_resp.response().code() == 0);
    assert(username_resp.user().user_id() == register_resp.user_id());
    assert(username_resp.user().username() == username);

    nebula::proto::LoginRequest bad_login_req;
    bad_login_req.set_request_id("bad-login");
    bad_login_req.set_username(username);
    bad_login_req.set_password("wrong-password");
    nebula::proto::LoginResponse bad_login_resp;
    assert(service.Login(&server_context, &bad_login_req, &bad_login_resp).ok());
    assert(bad_login_resp.response().code() == static_cast<int>(nebula::ErrorCode::AUTH_FAILED));

    nebula::proto::ValidateTokenRequest bad_token_req;
    bad_token_req.set_request_id("bad-token");
    bad_token_req.set_token("not-exists");
    nebula::proto::ValidateTokenResponse bad_token_resp;
    assert(service.ValidateToken(&server_context, &bad_token_req, &bad_token_resp).ok());
    assert(bad_token_resp.response().code() == 0);
    assert(!bad_token_resp.valid());

    return 0;
}
