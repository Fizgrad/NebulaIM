#include "DeviceServiceImpl.h"

#include "common/dao/DeviceDao.h"
#include "common/dao/UserDao.h"
#include "common/error/ErrorCode.h"
#include "common/log/Logger.h"
#include "common/monitor/MetricsRegistry.h"
#include "common/push/GatewayClient.h"
#include "common/push/GatewayClientManager.h"
#include "common/redis/RedisClient.h"
#include "common/rpc/InternalRpcAuth.h"
#include "common/utils/TimeUtil.h"

namespace nebula {

namespace {

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

void fillDevice(proto::DeviceInfo* info, const UserDevice& device, bool online) {
    info->set_user_id(device.user_id);
    info->set_device_id(device.device_id);
    info->set_platform(device.platform);
    info->set_device_name(device.device_name);
    info->set_last_login_at(device.last_login_at);
    info->set_last_active_at(device.last_active_at);
    info->set_online(online);
}

}  // namespace

DeviceServiceImpl::DeviceServiceImpl(UserDao* user_dao,
                                     DeviceDao* device_dao,
                                     RedisClient* redis_client,
                                     GatewayClientManager* gateway_client_manager)
    : user_dao_(user_dao),
      device_dao_(device_dao),
      redis_client_(redis_client),
      gateway_client_manager_(gateway_client_manager) {}

grpc::Status DeviceServiceImpl::ListDevices(grpc::ServerContext* context,
                                            const proto::ListDevicesRequest* request,
                                            proto::ListDevicesResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response->mutable_response())) return grpc::Status::OK;
    MetricsRegistry::instance().counter("nebula_device_list_total", "DeviceService list-devices requests").inc();
    if (user_dao_ == nullptr || device_dao_ == nullptr || redis_client_ == nullptr) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INTERNAL_ERROR);
        return grpc::Status::OK;
    }
    if (request->user_id() == 0) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::INVALID_ARGUMENT);
        return grpc::Status::OK;
    }
    if (!user_dao_->getUserById(request->user_id()).has_value()) {
        fillResponse(response->mutable_response(), request->request_id(), ErrorCode::USER_NOT_FOUND);
        return grpc::Status::OK;
    }
    for (const auto& device : device_dao_->listDevices(request->user_id())) {
        std::string gateway_id;
        std::string connection_id;
        fillDevice(response->add_devices(), device, isOnline(device.user_id, device.device_id, &gateway_id, &connection_id));
    }
    fillResponse(response->mutable_response(), request->request_id(), ErrorCode::OK, "OK");
    MetricsRegistry::instance().counter("nebula_device_list_success_total", "DeviceService successful list-devices requests").inc();
    return grpc::Status::OK;
}

grpc::Status DeviceServiceImpl::KickDevice(grpc::ServerContext* context,
                                           const proto::KickDeviceRequest* request,
                                           proto::CommonResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response)) return grpc::Status::OK;
    MetricsRegistry::instance().counter("nebula_device_kick_total", "DeviceService kick-device requests").inc();
    if (user_dao_ == nullptr || device_dao_ == nullptr || redis_client_ == nullptr) {
        fillResponse(response, request->request_id(), ErrorCode::INTERNAL_ERROR);
        return grpc::Status::OK;
    }
    if (request->user_id() == 0 || request->device_id().empty()) {
        fillResponse(response, request->request_id(), ErrorCode::INVALID_ARGUMENT);
        return grpc::Status::OK;
    }
    if (!user_dao_->getUserById(request->user_id()).has_value()) {
        fillResponse(response, request->request_id(), ErrorCode::USER_NOT_FOUND);
        return grpc::Status::OK;
    }
    auto device = device_dao_->getDevice(request->user_id(), request->device_id());
    if (!device.has_value()) {
        fillResponse(response, request->request_id(), ErrorCode::DEVICE_NOT_FOUND);
        return grpc::Status::OK;
    }
    if (!revokeDevice(request->request_id(), request->user_id(), request->device_id(), device->token_hash)) {
        fillResponse(response, request->request_id(), ErrorCode::SERVICE_UNAVAILABLE, "device connection revoke failed");
        return grpc::Status::OK;
    }
    fillResponse(response, request->request_id(), ErrorCode::OK, "OK");
    MetricsRegistry::instance().counter("nebula_device_kick_success_total", "DeviceService successful kick-device requests").inc();
    return grpc::Status::OK;
}

grpc::Status DeviceServiceImpl::KickAllDevices(grpc::ServerContext* context,
                                               const proto::KickAllDevicesRequest* request,
                                               proto::CommonResponse* response) {
    if (!requireInternalRpc(context, request->request_id(), response)) return grpc::Status::OK;
    MetricsRegistry::instance().counter("nebula_device_kick_all_total", "DeviceService kick-all-devices requests").inc();
    if (user_dao_ == nullptr || device_dao_ == nullptr || redis_client_ == nullptr) {
        fillResponse(response, request->request_id(), ErrorCode::INTERNAL_ERROR);
        return grpc::Status::OK;
    }
    if (request->user_id() == 0) {
        fillResponse(response, request->request_id(), ErrorCode::INVALID_ARGUMENT);
        return grpc::Status::OK;
    }
    if (!user_dao_->getUserById(request->user_id()).has_value()) {
        fillResponse(response, request->request_id(), ErrorCode::USER_NOT_FOUND);
        return grpc::Status::OK;
    }
    bool ok = true;
    for (const auto& device : device_dao_->listDevices(request->user_id())) {
        ok = revokeDevice(request->request_id(), request->user_id(), device.device_id, device.token_hash) && ok;
    }
    if (!ok) {
        fillResponse(response, request->request_id(), ErrorCode::SERVICE_UNAVAILABLE, "one or more device connections failed to revoke");
        return grpc::Status::OK;
    }
    fillResponse(response, request->request_id(), ErrorCode::OK, "OK");
    MetricsRegistry::instance().counter("nebula_device_kick_all_success_total", "DeviceService successful kick-all-devices requests").inc();
    return grpc::Status::OK;
}

bool DeviceServiceImpl::isOnline(uint64_t user_id, const std::string& device_id, std::string* gateway_id, std::string* connection_id) const {
    if (redis_client_ == nullptr || user_id == 0 || device_id.empty()) return false;
    auto gateway = redis_client_->get(onlineKey(user_id, device_id));
    auto conn = redis_client_->get(connKey(user_id, device_id));
    if (!gateway.has_value() || !conn.has_value()) {
        redis_client_->del(onlineKey(user_id, device_id));
        redis_client_->del(connKey(user_id, device_id));
        redis_client_->srem(devicesKey(user_id), device_id);
        return false;
    }
    if (gateway_id != nullptr) *gateway_id = gateway.value();
    if (connection_id != nullptr) *connection_id = conn.value();
    return true;
}

bool DeviceServiceImpl::revokeDevice(const std::string& request_id, uint64_t user_id, const std::string& device_id, const std::string& token_hash) {
    std::string gateway_id;
    std::string connection_id;
    bool live_kick_ok = true;
    if (isOnline(user_id, device_id, &gateway_id, &connection_id)) {
        GatewayClient* client = gateway_client_manager_ == nullptr ? nullptr : gateway_client_manager_->getClient(gateway_id);
        if (client == nullptr) {
            live_kick_ok = false;
        } else {
            live_kick_ok = client->kickConnection(request_id, user_id, connection_id, device_id, "device revoked");
        }
    }
    if (!token_hash.empty()) {
        redis_client_->del("nebula:token:" + token_hash);
    }
    redis_client_->del(onlineKey(user_id, device_id));
    redis_client_->del(connKey(user_id, device_id));
    redis_client_->srem(devicesKey(user_id), device_id);
    bool device_ok = device_dao_ == nullptr ? false : device_dao_->clearDeviceToken(user_id, device_id, TimeUtil::nowMs());
    LOG_INFO("device revoke user_id=" + std::to_string(user_id) +
             " device_id=" + device_id +
             " online_gateway=" + gateway_id +
             " connection_id=" + connection_id +
             " live_kick_ok=" + (live_kick_ok ? "true" : "false") +
             " device_ok=" + (device_ok ? "true" : "false"));
    if (!live_kick_ok) {
        LOG_WARN("device live connection kick failed after token revoke user_id=" + std::to_string(user_id) +
                 " device_id=" + device_id +
                 " gateway_id=" + gateway_id +
                 " connection_id=" + connection_id);
    }
    return device_ok;
}

std::string DeviceServiceImpl::devicesKey(uint64_t user_id) const { return "nebula:user:devices:" + std::to_string(user_id); }
std::string DeviceServiceImpl::onlineKey(uint64_t user_id, const std::string& device_id) const { return "nebula:user:online:" + std::to_string(user_id) + ":" + device_id; }
std::string DeviceServiceImpl::connKey(uint64_t user_id, const std::string& device_id) const { return "nebula:user:conn:" + std::to_string(user_id) + ":" + device_id; }

}  // namespace nebula
