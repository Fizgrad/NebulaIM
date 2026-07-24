#include "TestDeps.h"
#include "DeviceServiceContext.h"
#include "DeviceServiceImpl.h"
#include "common/auth/PasswordHasher.h"
#include "common/dao/DeviceDao.h"
#include "common/dao/UserDao.h"
#include "common/error/ErrorCode.h"
#include "common/redis/RedisClient.h"
#include "common/utils/TimeUtil.h"

#include <cassert>
#include <grpcpp/grpcpp.h>

namespace {

nebula::UserDevice makeDevice(uint64_t user_id, const std::string& device_id, const std::string& token_hash) {
    const int64_t now = nebula::TimeUtil::nowMs();
    nebula::UserDevice device;
    device.user_id = user_id;
    device.device_id = device_id;
    device.platform = "test";
    device.device_name = device_id;
    device.token_hash = token_hash;
    device.last_login_at = now;
    device.last_active_at = now;
    device.created_at = now;
    device.updated_at = now;
    return device;
}

}  // namespace

int main() {
    nebula::DeviceServiceContext context;
    if (!context.init("config/nebula.conf")) {
        return nebula::tests::skip("test_device_service_impl", "DeviceService dependencies are not reachable");
    }

    const int64_t now = nebula::TimeUtil::nowMs();
    nebula::User user;
    user.username = "device_svc_" + std::to_string(now);
    user.password_hash = nebula::PasswordHasher::hashPassword("password123");
    user.nickname = "Device Service Test";
    user.created_at = now;
    user.updated_at = now;
    uint64_t user_id = 0;
    assert(context.userDao()->createUser(user, &user_id));

    nebula::DeviceServiceImpl service(
        context.userDao(), context.deviceDao(), context.redisClient(), context.gatewayClientManager());

    const std::string offline_device_id = "offline-device";
    const std::string offline_token_hash = "offline-token-hash-" + std::to_string(user_id);
    assert(context.deviceDao()->upsertDevice(makeDevice(user_id, offline_device_id, offline_token_hash)));
    assert(context.redisClient()->setEx("nebula:token:" + offline_token_hash, 60, std::to_string(user_id)));

    grpc::ServerContext offline_server_context;
    nebula::proto::KickDeviceRequest offline_request;
    offline_request.set_request_id("kick-offline");
    offline_request.set_user_id(user_id);
    offline_request.set_device_id(offline_device_id);
    nebula::proto::CommonResponse offline_response;
    assert(service.KickDevice(&offline_server_context, &offline_request, &offline_response).ok());
    assert(offline_response.code() == 0);
    assert(!context.redisClient()->exists("nebula:token:" + offline_token_hash));
    auto offline_device = context.deviceDao()->getDevice(user_id, offline_device_id);
    assert(offline_device.has_value());
    assert(offline_device->token_hash.empty());

    const std::string live_device_id = "stale-live-device";
    const std::string live_token_hash = "live-token-hash-" + std::to_string(user_id);
    const std::string online_key = "nebula:user:online:" + std::to_string(user_id) + ":" + live_device_id;
    const std::string connection_key = "nebula:user:conn:" + std::to_string(user_id) + ":" + live_device_id;
    const std::string devices_key = "nebula:user:devices:" + std::to_string(user_id);
    assert(context.deviceDao()->upsertDevice(makeDevice(user_id, live_device_id, live_token_hash)));
    assert(context.redisClient()->setEx("nebula:token:" + live_token_hash, 60, std::to_string(user_id)));
    assert(context.redisClient()->setEx(online_key, 60, "missing-gateway"));
    assert(context.redisClient()->setEx(connection_key, 60, "missing-connection"));
    assert(context.redisClient()->sadd(devices_key, live_device_id));

    grpc::ServerContext live_server_context;
    nebula::proto::KickDeviceRequest live_request;
    live_request.set_request_id("kick-stale-live");
    live_request.set_user_id(user_id);
    live_request.set_device_id(live_device_id);
    nebula::proto::CommonResponse live_response;
    assert(service.KickDevice(&live_server_context, &live_request, &live_response).ok());
    assert(live_response.code() == static_cast<int>(nebula::ErrorCode::SERVICE_UNAVAILABLE));
    assert(!context.redisClient()->exists("nebula:token:" + live_token_hash));
    assert(!context.redisClient()->exists(online_key));
    assert(!context.redisClient()->exists(connection_key));
    auto live_device = context.deviceDao()->getDevice(user_id, live_device_id);
    assert(live_device.has_value());
    assert(live_device->token_hash.empty());

    return 0;
}
