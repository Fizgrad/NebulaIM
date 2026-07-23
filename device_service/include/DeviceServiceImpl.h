#pragma once

#include "device.grpc.pb.h"

#include <string>

namespace nebula {

class DeviceDao;
class GatewayClientManager;
class RedisClient;
class UserDao;

class DeviceServiceImpl final : public proto::DeviceService::Service {
public:
    DeviceServiceImpl(UserDao* user_dao,
                      DeviceDao* device_dao,
                      RedisClient* redis_client,
                      GatewayClientManager* gateway_client_manager);

    grpc::Status ListDevices(grpc::ServerContext* context,
                             const proto::ListDevicesRequest* request,
                             proto::ListDevicesResponse* response) override;

    grpc::Status KickDevice(grpc::ServerContext* context,
                            const proto::KickDeviceRequest* request,
                            proto::CommonResponse* response) override;

    grpc::Status KickAllDevices(grpc::ServerContext* context,
                                const proto::KickAllDevicesRequest* request,
                                proto::CommonResponse* response) override;

private:
    bool isOnline(uint64_t user_id, const std::string& device_id, std::string* gateway_id, std::string* connection_id) const;
    bool revokeDevice(const std::string& request_id, uint64_t user_id, const std::string& device_id, const std::string& token_hash);

    std::string devicesKey(uint64_t user_id) const;
    std::string onlineKey(uint64_t user_id, const std::string& device_id) const;
    std::string connKey(uint64_t user_id, const std::string& device_id) const;

    UserDao* user_dao_;
    DeviceDao* device_dao_;
    RedisClient* redis_client_;
    GatewayClientManager* gateway_client_manager_;
};

}  // namespace nebula
