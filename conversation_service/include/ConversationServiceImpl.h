#pragma once

#include "conversation.grpc.pb.h"

namespace nebula {

class ConversationDao;

class ConversationServiceImpl final : public proto::ConversationService::Service {
public:
    explicit ConversationServiceImpl(ConversationDao* conversation_dao);

    grpc::Status ListConversations(grpc::ServerContext* context,
                                   const proto::ListConversationsRequest* request,
                                   proto::ListConversationsResponse* response) override;
    grpc::Status DeleteConversation(grpc::ServerContext* context,
                                    const proto::ConversationDeleteRequest* request,
                                    proto::CommonResponse* response) override;
    grpc::Status PinConversation(grpc::ServerContext* context,
                                 const proto::ConversationPinRequest* request,
                                 proto::CommonResponse* response) override;
    grpc::Status MuteConversation(grpc::ServerContext* context,
                                  const proto::ConversationMuteRequest* request,
                                  proto::CommonResponse* response) override;

private:
    ConversationDao* conversation_dao_;
};

}  // namespace nebula
