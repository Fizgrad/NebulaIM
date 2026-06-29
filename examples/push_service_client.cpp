#include "common/config/Config.h"
#include "common/dao/GroupDao.h"
#include "common/dao/UserDao.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/push/OnlineStatusManager.h"
#include "common/redis/RedisClient.h"
#include "common/utils/TimeUtil.h"
#include "push.grpc.pb.h"

#include <grpcpp/grpcpp.h>
#include <iostream>

namespace {
std::string parseAddress(int argc, char** argv) {
    std::string address = "127.0.0.1:50054";
    for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "--addr" && i + 1 < argc) address = argv[++i];
    return address;
}
}

int main(int argc, char** argv) {
    nebula::Config config;
    if (!config.loadFromFile("config/nebula.conf")) return 1;
    nebula::RedisConfig redis_config;
    redis_config.host = config.getString("redis.host", redis_config.host);
    redis_config.port = config.getInt("redis.port", redis_config.port);
    nebula::RedisClient redis;
    redis.connect(redis_config);
    nebula::OnlineStatusManager online(&redis);
    online.setOnline(10001, "gateway-1", "conn-10001", 60);

    auto stub = nebula::proto::PushService::NewStub(grpc::CreateChannel(parseAddress(argc, argv), grpc::InsecureChannelCredentials()));
    nebula::proto::MessageData message;
    message.set_message_id(static_cast<uint64_t>(nebula::TimeUtil::nowMs()));
    message.set_conversation_id(1);
    message.set_from_user_id(10000);
    message.set_to_user_id(10001);
    message.set_content_type(nebula::proto::MESSAGE_CONTENT_TYPE_TEXT);
    message.set_content("push client message");
    message.set_status(nebula::proto::MESSAGE_STATUS_SENT);
    message.set_timestamp(nebula::TimeUtil::nowMs());

    nebula::proto::PushToUserRequest req;
    req.set_request_id("push-client-user");
    req.set_user_id(10001);
    *req.mutable_message() = message;
    nebula::proto::PushToUserResponse resp;
    grpc::ClientContext ctx;
    stub->PushToUser(&ctx, req, &resp);
    std::cout << "PushToUser code=" << resp.response().code() << std::endl;
    return 0;
}
