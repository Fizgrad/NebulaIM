#include "UserServiceImpl.h"

#include "common/auth/PasswordHasher.h"
#include "common/auth/TokenManager.h"
#include "common/dao/DeviceDao.h"
#include "common/dao/UserDao.h"
#include "common/error/ErrorCode.h"
#include "common/log/Logger.h"
#include "common/monitor/MetricsRegistry.h"
#include "common/redis/RedisClient.h"
#include "common/rpc/InternalRpcAuth.h"
#include "common/utils/TimeUtil.h"

#include <cstddef>

namespace nebula {

namespace {

constexpr size_t kMaxUsernameLength = 64;
constexpr size_t kMaxPasswordLength = 256;
constexpr size_t kMaxNicknameLength = 64;
constexpr size_t kMaxDeviceIdLength = 128;
constexpr size_t kMaxPlatformLength = 32;
constexpr size_t kMaxDeviceNameLength = 128;
constexpr size_t kMaxTokenLength = 256;

void fillResponse(proto::CommonResponse* response, const std::string& request_id, ErrorCode code, const std::string& message = "") {
    response->set_code(static_cast<int32_t>(code));
    response->set_message(message.empty() ? errorCodeToString(code) : message);
    response->set_request_id(request_id.empty() ? "request-id-empty" : request_id);
}

bool requireInternalRpc(grpc::ServerContext* context, const std::string& request_id, proto::CommonResponse* response) {
    if (InternalRpcAuth::instance().authorize(context)) return true;
    fillResponse(response, request_id, ErrorCode::AUTH_FAILED, "internal rpc unauthenticated");
    return false;
}

void fillUserInfo(proto::UserInfo* info, const User& user) {
    info->set_user_id(user.id);
    info->set_username(user.username);
    info->set_nickname(user.nickname);
    info->set_avatar(user.avatar);
    info->set_created_at(user.created_at);
}

}  // namespace

UserServiceImpl::UserServiceImpl(UserDao* user_dao,
                                 DeviceDao* device_dao,
                                 RedisClient* redis_client,
                                 TokenManager* token_manager,
                                 int password_min_length)
    : user_dao_(user_dao),
      device_dao_(device_dao),
      redis_client_(redis_client),
      token_manager_(token_manager),
      password_min_length_(password_min_length > 0 ? password_min_length : 6) {}

grpc::Status UserServiceImpl::Register(grpc::ServerContext* context,
                                       const proto::RegisterRequest* request,
                                       proto::RegisterResponse* response) {
    LOG_INFO("Register request");
    MetricsRegistry::instance().counter("nebula_user_register_total", "UserService register requests").inc();
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
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
    if (request->username().size() > kMaxUsernameLength ||
        request->password().size() > kMaxPasswordLength ||
        request->nickname().size() > kMaxNicknameLength) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT, "registration field exceeds size limit");
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
    MetricsRegistry::instance().counter("nebula_user_register_success_total", "UserService successful registrations").inc();
    return grpc::Status::OK;
}

grpc::Status UserServiceImpl::Login(grpc::ServerContext* context,
                                    const proto::LoginRequest* request,
                                    proto::LoginResponse* response) {
    LOG_INFO("Login request");
    MetricsRegistry::instance().counter("nebula_user_login_total", "UserService login requests").inc();
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
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
    if (request->username().size() > kMaxUsernameLength ||
        request->password().size() > kMaxPasswordLength ||
        request->device_id().size() > kMaxDeviceIdLength ||
        request->platform().size() > kMaxPlatformLength ||
        request->device_name().size() > kMaxDeviceNameLength) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT, "login field exceeds size limit");
        return grpc::Status::OK;
    }

    auto user = user_dao_->getUserByUsername(request->username());
    if (!user.has_value() || !PasswordHasher::verifyPassword(request->password(), user->password_hash)) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::AUTH_FAILED, "auth failed");
        MetricsRegistry::instance().counter("nebula_user_login_failed_total", "UserService failed logins").inc();
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
    if (device_dao_ != nullptr && !request->device_id().empty()) {
        int64_t now = TimeUtil::nowMs();
        std::string token_hash = TokenManager::tokenHash(token);
        auto existing_device = device_dao_->getDevice(user->id, request->device_id());
        if (existing_device.has_value() && !existing_device->token_hash.empty() && existing_device->token_hash != token_hash) {
            redis_client_->del("nebula:token:" + existing_device->token_hash);
        }
        UserDevice device;
        device.user_id = user->id;
        device.device_id = request->device_id();
        device.platform = request->platform();
        device.device_name = request->device_name();
        device.token_hash = token_hash;
        device.last_login_at = now;
        device.last_active_at = now;
        device.created_at = now;
        device.updated_at = now;
        if (!device_dao_->upsertDevice(device)) {
            redis_client_->del(token_manager_->tokenKey(token));
            fillResponse(response->mutable_response(), request->request_id(), ErrorCode::DB_ERROR, "device metadata persist failed");
            return grpc::Status::OK;
        }
    }

    LOG_INFO("Login success user_id=" + std::to_string(user->id));
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    response->set_user_id(user->id);
    response->set_token(token);
    response->set_expire_at(token_manager_->expireAtMs());
    MetricsRegistry::instance().counter("nebula_user_login_success_total", "UserService successful logins").inc();
    return grpc::Status::OK;
}

grpc::Status UserServiceImpl::ValidateToken(grpc::ServerContext* context,
                                            const proto::ValidateTokenRequest* request,
                                            proto::ValidateTokenResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
    if (redis_client_ == nullptr || token_manager_ == nullptr) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR);
        return grpc::Status::OK;
    }
    if (request->token().empty() || request->token().size() > kMaxTokenLength) {
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

grpc::Status UserServiceImpl::GetUserInfo(grpc::ServerContext* context,
                                          const proto::GetUserInfoRequest* request,
                                          proto::GetUserInfoResponse* response) {
    LOG_INFO("GetUserInfo user_id=" + std::to_string(request->user_id()));
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
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

grpc::Status UserServiceImpl::GetUserByUsername(grpc::ServerContext* context,
                                                const proto::GetUserByUsernameRequest* request,
                                                proto::GetUserInfoResponse* response) {
    LOG_INFO("GetUserByUsername request");
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
    if (user_dao_ == nullptr) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR);
        return grpc::Status::OK;
    }
    if (request->username().empty()) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::USERNAME_EMPTY);
        return grpc::Status::OK;
    }
    if (request->username().size() > kMaxUsernameLength) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT, "username exceeds size limit");
        return grpc::Status::OK;
    }

    auto user = user_dao_->getUserByUsername(request->username());
    if (!user.has_value()) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::USER_NOT_FOUND);
        return grpc::Status::OK;
    }

    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    fillUserInfo(response->mutable_user(), user.value());
    return grpc::Status::OK;
}

grpc::Status UserServiceImpl::RefreshToken(grpc::ServerContext* context, const proto::RefreshTokenRequest* request, proto::RefreshTokenResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
    if (redis_client_ == nullptr || token_manager_ == nullptr) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR);
        return grpc::Status::OK;
    }
    if (request->token().empty() ||
        request->token().size() > kMaxTokenLength ||
        request->device_id().size() > kMaxDeviceIdLength) {
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
    std::string previous_device_token_hash;
    std::string new_token_hash = TokenManager::tokenHash(new_token);
    if (device_dao_ != nullptr && !request->device_id().empty()) {
        auto device = device_dao_->getDevice(user_id, request->device_id());
        if (device.has_value()) {
            previous_device_token_hash = device->token_hash;
            int64_t now = TimeUtil::nowMs();
            device->token_hash = new_token_hash;
            device->last_active_at = now;
            device->updated_at = now;
            if (!device_dao_->upsertDevice(*device)) {
                redis_client_->del(token_manager_->tokenKey(new_token));
                fillResponse(response->mutable_response(), request->request_id(), ErrorCode::TOKEN_REFRESH_FAILED);
                return grpc::Status::OK;
            }
        }
    }
    if (!previous_device_token_hash.empty() && previous_device_token_hash != new_token_hash) {
        redis_client_->del("nebula:token:" + previous_device_token_hash);
    }
    redis_client_->del(token_manager_->tokenKey(request->token()));

    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    response->set_user_id(user_id);
    response->set_token(new_token);
    response->set_expire_at(token_manager_->expireAtMs());
    return grpc::Status::OK;
}

}  // namespace nebula
