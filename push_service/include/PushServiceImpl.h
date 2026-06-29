#pragma once

#include "common/push/PushDispatcher.h"
#include "push.grpc.pb.h"

namespace nebula {

class PushServiceImpl final : public proto::PushService::Service {
public:
    explicit PushServiceImpl(PushDispatcher* dispatcher);

    grpc::Status PushToUser(grpc::ServerContext* context,
                            const proto::PushToUserRequest* request,
                            proto::PushToUserResponse* response) override;
    grpc::Status PushToGroup(grpc::ServerContext* context,
                             const proto::PushToGroupRequest* request,
                             proto::PushToGroupResponse* response) override;

private:
    PushDispatcher* dispatcher_;
};

}  // namespace nebula
