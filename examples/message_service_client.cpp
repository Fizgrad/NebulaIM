#include "common/auth/PasswordHasher.h"
#include "common/config/Config.h"
#include "common/dao/GroupDao.h"
#include "common/dao/UserDao.h"
#include "common/db/MySqlConnectionPool.h"
#include "common/utils/TimeUtil.h"
#include "message.grpc.pb.h"

#include <grpcpp/grpcpp.h>
#include <iostream>
#include <string>

namespace {
std::string parseAddress(int argc, char** argv) {
    std::string address = "127.0.0.1:50052";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--addr" && i + 1 < argc) address = argv[++i];
    }
    return address;
}

uint64_t createUser(nebula::UserDao& dao, const std::string& prefix) {
    int64_t now = nebula::TimeUtil::nowMs();
    nebula::User user;
    user.username = prefix + std::to_string(now);
    user.password_hash = nebula::PasswordHasher::hashPassword("password123");
    user.nickname = prefix;
    user.created_at = now;
    user.updated_at = now;
    uint64_t id = 0;
    dao.createUser(user, &id);
    return id;
}
}

int main(int argc, char** argv) {
    nebula::Config config;
    if (!config.loadFromFile("config/nebula.conf")) return 1;
    nebula::MySqlConfig mysql;
    mysql.host = config.getString("mysql.host", mysql.host);
    mysql.port = config.getInt("mysql.port", mysql.port);
    mysql.user = config.getString("mysql.user", mysql.user);
    mysql.password = config.getString("mysql.password", mysql.password);
    mysql.database = config.getString("mysql.database", mysql.database);
    nebula::MySqlConnectionPool pool;
    if (!pool.init(mysql, 2)) return 1;
    nebula::UserDao user_dao(pool);
    nebula::GroupDao group_dao(pool);
    uint64_t u1 = createUser(user_dao, "msg_client_u1_");
    uint64_t u2 = createUser(user_dao, "msg_client_u2_");

    auto stub = nebula::proto::MessageService::NewStub(grpc::CreateChannel(parseAddress(argc, argv), grpc::InsecureChannelCredentials()));

    nebula::proto::SendSingleMessageRequest single;
    single.set_request_id("client-single");
    single.set_from_user_id(u1);
    single.set_to_user_id(u2);
    single.set_content_type(nebula::proto::MESSAGE_CONTENT_TYPE_TEXT);
    single.set_content("hello single");
    single.set_client_sequence_id(1001);
    nebula::proto::SendSingleMessageResponse single_resp;
    grpc::ClientContext single_ctx;
    stub->SendSingleMessage(&single_ctx, single, &single_resp);
    std::cout << "SendSingleMessage code=" << single_resp.response().code() << " message_id=" << single_resp.message_id() << std::endl;

    nebula::proto::SendSingleMessageResponse dup_resp;
    grpc::ClientContext dup_ctx;
    stub->SendSingleMessage(&dup_ctx, single, &dup_resp);
    std::cout << "Dedup message_id=" << dup_resp.message_id() << std::endl;

    int64_t now = nebula::TimeUtil::nowMs();
    nebula::Group group;
    group.group_name = "msg_client_group_" + std::to_string(now);
    group.owner_id = u1;
    group.created_at = now;
    group.updated_at = now;
    uint64_t group_id = 0;
    group_dao.createGroup(group, &group_id);

    nebula::proto::SendGroupMessageRequest group_req;
    group_req.set_request_id("client-group");
    group_req.set_from_user_id(u1);
    group_req.set_group_id(group_id);
    group_req.set_content_type(nebula::proto::MESSAGE_CONTENT_TYPE_TEXT);
    group_req.set_content("hello group");
    group_req.set_client_sequence_id(1002);
    nebula::proto::SendGroupMessageResponse group_resp;
    grpc::ClientContext group_ctx;
    stub->SendGroupMessage(&group_ctx, group_req, &group_resp);
    std::cout << "SendGroupMessage code=" << group_resp.response().code() << " message_id=" << group_resp.message_id() << std::endl;

    nebula::proto::AckMessageRequest ack;
    ack.set_request_id("client-ack");
    ack.set_user_id(u2);
    ack.set_message_id(single_resp.message_id());
    nebula::proto::AckMessageResponse ack_resp;
    grpc::ClientContext ack_ctx;
    stub->AckMessage(&ack_ctx, ack, &ack_resp);
    std::cout << "AckMessage code=" << ack_resp.response().code() << std::endl;

    nebula::proto::PullOfflineMessagesRequest pull;
    pull.set_request_id("client-pull");
    pull.set_user_id(u2);
    pull.mutable_page()->set_page_size(10);
    nebula::proto::PullOfflineMessagesResponse pull_resp;
    grpc::ClientContext pull_ctx;
    stub->PullOfflineMessages(&pull_ctx, pull, &pull_resp);
    std::cout << "PullOfflineMessages code=" << pull_resp.response().code() << " count=" << pull_resp.messages_size() << std::endl;
    return 0;
}
