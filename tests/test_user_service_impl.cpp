#include "TestDeps.h"
#include "UserServiceContext.h"
#include "UserServiceImpl.h"
#include "common/auth/TokenManager.h"
#include "common/error/ErrorCode.h"
#include "common/utils/TimeUtil.h"

#include <cassert>
#include <grpcpp/grpcpp.h>

int main() {
    nebula::UserServiceContext context;
    if (!context.init("config/nebula.conf")) return nebula::tests::skip("test_user_service_impl", "UserService dependencies are not reachable");

    nebula::UserServiceImpl service(context.userDao(), context.deviceDao(), context.redisClient(), context.tokenManager(), context.passwordMinLength());
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
    login_req.set_device_id("svc-device");
    login_req.set_platform("test");
    login_req.set_device_name("svc-device");
    nebula::proto::LoginResponse login_resp;
    assert(service.Login(&server_context, &login_req, &login_resp).ok());
    assert(login_resp.response().code() == 0);
    assert(login_resp.user_id() == register_resp.user_id());
    assert(!login_resp.token().empty());
    auto login_device = context.deviceDao()->getDevice(register_resp.user_id(), "svc-device");
    assert(login_device.has_value());
    assert(login_device->token_hash == nebula::TokenManager::tokenHash(login_resp.token()));

    nebula::proto::ValidateTokenRequest validate_req;
    validate_req.set_request_id("validate-1");
    validate_req.set_token(login_resp.token());
    nebula::proto::ValidateTokenResponse validate_resp;
    assert(service.ValidateToken(&server_context, &validate_req, &validate_resp).ok());
    assert(validate_resp.response().code() == 0);
    assert(validate_resp.valid());
    assert(validate_resp.user_id() == register_resp.user_id());

    nebula::proto::LoginResponse second_login_resp;
    assert(service.Login(&server_context, &login_req, &second_login_resp).ok());
    assert(second_login_resp.response().code() == 0);
    assert(second_login_resp.user_id() == register_resp.user_id());
    assert(second_login_resp.token() != login_resp.token());

    nebula::proto::ValidateTokenResponse old_login_token_resp;
    assert(service.ValidateToken(&server_context, &validate_req, &old_login_token_resp).ok());
    assert(old_login_token_resp.response().code() == 0);
    assert(!old_login_token_resp.valid());

    nebula::proto::RefreshTokenRequest refresh_req;
    refresh_req.set_request_id("refresh-1");
    refresh_req.set_token(second_login_resp.token());
    refresh_req.set_device_id("svc-device");
    nebula::proto::RefreshTokenResponse refresh_resp;
    assert(service.RefreshToken(&server_context, &refresh_req, &refresh_resp).ok());
    assert(refresh_resp.response().code() == 0);
    assert(refresh_resp.user_id() == register_resp.user_id());
    assert(!refresh_resp.token().empty());
    assert(refresh_resp.token() != second_login_resp.token());
    auto refreshed_device = context.deviceDao()->getDevice(register_resp.user_id(), "svc-device");
    assert(refreshed_device.has_value());
    assert(refreshed_device->token_hash == nebula::TokenManager::tokenHash(refresh_resp.token()));

    nebula::proto::ValidateTokenRequest second_token_req;
    second_token_req.set_request_id("validate-second-token");
    second_token_req.set_token(second_login_resp.token());
    nebula::proto::ValidateTokenResponse second_token_resp;
    assert(service.ValidateToken(&server_context, &second_token_req, &second_token_resp).ok());
    assert(second_token_resp.response().code() == 0);
    assert(!second_token_resp.valid());

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
