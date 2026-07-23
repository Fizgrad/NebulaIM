#include "BenchArgs.h"
#include "GatewayBenchClient.h"
#include "Stats.h"

#include "common/rpc/RpcMetadata.h"
#include "relation.grpc.pb.h"

#include <chrono>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <string>

namespace {

uint64_t createGroup(const std::string& relation_addr,
                     const std::string& internal_token,
                     const std::string& request_id,
                     uint64_t owner_id,
                     const std::string& group_name,
                     std::string* error) {
    auto stub = nebula::proto::RelationService::NewStub(grpc::CreateChannel(relation_addr, grpc::InsecureChannelCredentials()));
    grpc::ClientContext context;
    if (!internal_token.empty()) nebula::RpcMetadata::injectInternalToken(&context, internal_token);
    nebula::proto::CreateGroupRequest request;
    request.set_request_id(request_id);
    request.set_owner_id(owner_id);
    request.set_group_name(group_name);
    nebula::proto::CreateGroupResponse response;
    grpc::Status status = stub->CreateGroup(&context, request, &response);
    if (!status.ok()) {
        if (error != nullptr) *error = status.error_message();
        return 0;
    }
    if (response.response().code() != 0) {
        if (error != nullptr) *error = response.response().message();
        return 0;
    }
    return response.group_id();
}

bool joinGroup(const std::string& relation_addr,
               const std::string& internal_token,
               const std::string& request_id,
               uint64_t user_id,
               uint64_t group_id,
               std::string* error) {
    auto stub = nebula::proto::RelationService::NewStub(grpc::CreateChannel(relation_addr, grpc::InsecureChannelCredentials()));
    grpc::ClientContext context;
    if (!internal_token.empty()) nebula::RpcMetadata::injectInternalToken(&context, internal_token);
    nebula::proto::JoinGroupRequest request;
    request.set_request_id(request_id);
    request.set_user_id(user_id);
    request.set_group_id(group_id);
    nebula::proto::CommonResponse response;
    grpc::Status status = stub->JoinGroup(&context, request, &response);
    if (!status.ok()) {
        if (error != nullptr) *error = status.error_message();
        return false;
    }
    if (response.code() != 0) {
        if (error != nullptr) *error = response.message();
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    std::string host = nebula::benchmark::argValue(argc, argv, "--host", "127.0.0.1");
    uint16_t port = nebula::benchmark::portArg(argc, argv, "--port", 9000);
    int requests = nebula::benchmark::intArg(argc, argv, "--requests", 1000);
    std::string relation_addr = nebula::benchmark::argValue(argc, argv, "--relation-addr", "127.0.0.1:50053");
    std::string internal_token = nebula::benchmark::argValue(argc, argv, "--internal-token", "");
    uint64_t group_id = nebula::benchmark::uint64Arg(argc, argv, "--group-id", 0);
    std::string password = nebula::benchmark::argValue(argc, argv, "--password", "password123");
    std::string run = nebula::benchmark::runId();

    std::string error;
    nebula::benchmark::GatewayBenchClient sender(host, port);
    uint64_t sender_id = 0;
    nebula::benchmark::LoginResult login;
    std::string username = "bench_group_sender_" + run;
    if (!sender.connectGateway(&error) ||
        !sender.registerUser(username, password, &sender_id, &error) ||
        !sender.loginUser(username, password, "bench-group-sender-" + run, &login, &error)) {
        std::cerr << "sender setup failed: " << error << "\n";
        return 2;
    }

    if (group_id == 0) {
        group_id = createGroup(relation_addr, internal_token, "bench-create-group-" + run, sender_id, "bench_group_" + run, &error);
        if (group_id == 0) {
            std::cerr << "group setup failed: " << error << "\n";
            return 2;
        }
    } else if (!joinGroup(relation_addr, internal_token, "bench-join-group-" + run, sender_id, group_id, &error)) {
        std::cerr << "group join failed: " << error << "\n";
        return 2;
    }

    nebula::benchmark::Stats stats("group_message");
    stats.start();
    for (int i = 0; i < requests; ++i) {
        uint64_t message_id = 0;
        auto started = std::chrono::steady_clock::now();
        bool ok = sender.sendGroupMessage(sender_id, group_id, "bench group message", static_cast<uint32_t>(300000 + i), &message_id, &error);
        double latency = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        stats.recordLatency(latency);
        if (ok && message_id != 0) stats.incSuccess();
        else stats.incFailure();
    }
    std::cout << stats.report();
    return 0;
}
