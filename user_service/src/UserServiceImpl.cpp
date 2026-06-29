#include "UserServiceImpl.h"

#include "common/auth/PasswordHasher.h"
#include "common/auth/TokenManager.h"
#include "common/dao/UserDao.h"
#include "common/error/ErrorCode.h"
#include "common/log/Logger.h"
#include "common/redis/RedisClient.h"
#include "common/utils/TimeUtil.h"

namespace nebula {

namespace {

void fillResponse(proto::CommonResponse* response, const std::string& request_id, ErrorCode code, const std::string& message = "") {
    response->set_code(static_cast<int32_t>(code));
    response->set_message(message.empty() ? errorCodeToString(code) : message);
    response->set_request_id(request_id.empty() ? "request-id-empty" : request_id);
}

void fillUserInfo(proto::UserInfo* info, const User& user) {
    info->set_user_id(user.id);
    info->set_username(user.username);
    info->set_nickname(user.nickname);
    info->set_avatar(user.avatar);
    info->set_created_at(user.created_at);
}

std::string tokenPrefix(const std::string& token) {
    if (token.size() <= 8) {
        return token;
    }
    return token.substr(0, 8);
}

}  // namespace

UserServiceImpl::UserServiceImpl(UserDao* user_dao,
                                 RedisClient* redis_client,
                                 TokenManager* token_manager,
                                 int password_min_length)
    : user_dao_(user_dao),
      redis_client_(redis_client),
      token_manager_(token_manager),
      password_min_length_(password_min_length > 0 ? password_min_length : 6) {}

grpc::Status UserServiceImpl::Register(grpc::ServerContext*,
                                       const proto::RegisterRequest* request,
                                       proto::RegisterResponse* response) {
    LOG_INFO("Register username=" + request->username());
    if (user_dao_ == nullptr) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR);
        return grpc::Status::OK;
    }
    if (request->username().empty()) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::USERNAME_EMPTY);
        return grpc::Status::OK;
    }
    if (request->password().empty()) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::PASSWORD_EMPTY);
        return grpc::Status::OK;
    }
    if (static_cast<int>(request->password().size()) < password_min_length_) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::PASSWORD_TOO_SHORT);
        return grpc::Status::OK;
    }
    if (user_dao_->getUserByUsername(request->username()).has_value()) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::USER_ALREADY_EXISTS);
        return grpc::Status::OK;
    }

    std::string password_hash = PasswordHasher::hashPassword(request->password());
    if (password_hash.empty()) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR);
        return grpc::Status::OK;
    }

    int64_t now = TimeUtil::nowMs();
    User user;
    user.username = request->username();
    user.password_hash = password_hash;
    user.nickname = request->nickname();
    user.created_at = now;
    user.updated_at = now;

    uint64_t user_id = 0;
    if (!user_dao_->createUser(user, &user_id)) {
        if (user_dao_->getUserByUsername(request->username()).has_value()) {
            fillResponse(response->mutable_response(), request->request_id(), ErrorCode::USER_ALREADY_EXISTS);
        } else {
            fillResponse(response->mutable_response(), request->request_id(), ErrorCode::DB_ERROR);
        }
        return grpc::Status::OK;
    }

    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    response->set_user_id(user_id);
    return grpc::Status::OK;
}

grpc::Status UserServiceImpl::Login(grpc::ServerContext*,
                                    const proto::LoginRequest* request,
                                    proto::LoginResponse* response) {
    LOG_INFO("Login username=" + request->username());
    if (user_dao_ == nullptr || redis_client_ == nullptr || token_manager_ == nullptr) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR);
        return grpc::Status::OK;
    }
    if (request->username().empty()) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::USERNAME_EMPTY);
        return grpc::Status::OK;
    }
    if (request->password().empty()) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::PASSWORD_EMPTY);
        return grpc::Status::OK;
    }

    auto user = user_dao_->getUserByUsername(request->username());
    if (!user.has_value() || !PasswordHasher::verifyPassword(request->password(), user->password_hash)) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::AUTH_FAILED, "auth failed");
        return grpc::Status::OK;
    }

    std::string token = token_manager_->generateToken(user->id);
    if (token.empty()) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR);
        return grpc::Status::OK;
    }
    if (!redis_client_->setEx(token_manager_->tokenKey(token), token_manager_->ttlSeconds(), std::to_string(user->id))) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::REDIS_ERROR);
        return grpc::Status::OK;
    }

    LOG_INFO("Login success user_id=" + std::to_string(user->id) + " token_prefix=" + tokenPrefix(token));
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    response->set_user_id(user->id);
    response->set_token(token);
    response->set_expire_at(token_manager_->expireAtMs());
    return grpc::Status::OK;
}

grpc::Status UserServiceImpl::ValidateToken(grpc::ServerContext*,
                                            const proto::ValidateTokenRequest* request,
                                            proto::ValidateTokenResponse* response) {
    LOG_INFO("ValidateToken token_prefix=" + tokenPrefix(request->token()));
    if (redis_client_ == nullptr || token_manager_ == nullptr) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR);
        return grpc::Status::OK;
    }
    if (request->token().empty()) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::TOKEN_INVALID);
        response->set_valid(false);
        return grpc::Status::OK;
    }

    auto value = redis_client_->get(token_manager_->tokenKey(request->token()));
    if (!value.has_value()) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
        response->set_valid(false);
        response->set_user_id(0);
        return grpc::Status::OK;
    }

    uint64_t user_id = 0;
    try {
        user_id = static_cast<uint64_t>(std::stoull(value.value()));
    } catch (...) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::TOKEN_INVALID);
        response->set_valid(false);
        return grpc::Status::OK;
    }

    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    response->set_valid(true);
    response->set_user_id(user_id);
    return grpc::Status::OK;
}

grpc::Status UserServiceImpl::GetUserInfo(grpc::ServerContext*,
                                          const proto::GetUserInfoRequest* request,
                                          proto::GetUserInfoResponse* response) {
    LOG_INFO("GetUserInfo user_id=" + std::to_string(request->user_id()));
    if (user_dao_ == nullptr) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR);
        return grpc::Status::OK;
    }
    if (request->user_id() == 0) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT);
        return grpc::Status::OK;
    }

    auto user = user_dao_->getUserById(request->user_id());
    if (!user.has_value()) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::USER_NOT_FOUND);
        return grpc::Status::OK;
    }

    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    fillUserInfo(response->mutable_user(), user.value());
    return grpc::Status::OK;
}

grpc::Status UserServiceImpl::Logout(grpc::ServerContext*, const proto::LogoutRequest* request, proto::CommonResponse* response) {
    LOG_INFO("Logout token_prefix=" + tokenPrefix(request->token()));
    if (redis_client_ == nullptr || token_manager_ == nullptr) {
        fillResponse(response, request->request_id(), ErrorCode::INTERNAL_ERROR);
        return grpc::Status::OK;
    }
    if (request->token().empty()) {
        fillResponse(response, request->request_id(), ErrorCode::TOKEN_INVALID);
        return grpc::Status::OK;
    }
    bool ok = redis_client_->del(token_manager_->tokenKey(request->token()));
    if (!ok) {
        fillResponse(response, request->request_id(), ErrorCode::LOGOUT_FAILED);
        return grpc::Status::OK;
    }
    fillResponse(response, request->request_id(), ErrorCode::OK, "OK");
    return grpc::Status::OK;
}

grpc::Status UserServiceImpl::RefreshToken(grpc::ServerContext*, const proto::RefreshTokenRequest* request, proto::RefreshTokenResponse* response) {
    LOG_INFO("RefreshToken token_prefix=" + tokenPrefix(request->token()));
    if (redis_client_ == nullptr || token_manager_ == nullptr) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR);
        return grpc::Status::OK;
    }
    if (request->token().empty()) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::TOKEN_INVALID);
        return grpc::Status::OK;
    }
    auto value = redis_client_->get(token_manager_->tokenKey(request->token()));
    if (!value.has_value()) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::TOKEN_REFRESH_FAILED);
        return grpc::Status::OK;
    }

    uint64_t user_id = 0;
    try {
        user_id = static_cast<uint64_t>(std::stoull(value.value()));
    } catch (...) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::TOKEN_INVALID);
        return grpc::Status::OK;
    }

    std::string new_token = token_manager_->generateToken(user_id);
    if (new_token.empty() ||
        !redis_client_->setEx(token_manager_->tokenKey(new_token), token_manager_->ttlSeconds(), std::to_string(user_id))) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::TOKEN_REFRESH_FAILED);
        return grpc::Status::OK;
    }
    redis_client_->del(token_manager_->tokenKey(request->token()));

    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    response->set_user_id(user_id);
    response->set_token(new_token);
    response->set_expire_at(token_manager_->expireAtMs());
    return grpc::Status::OK;
}

}  // namespace nebula
