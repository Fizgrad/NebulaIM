#pragma once

#include "MessageServiceContext.h"
#include "message.grpc.pb.h"

namespace nebula {

class MessageServiceImpl final : public proto::MessageService::Service {
public:
    MessageServiceImpl(UserDao* user_dao,
                       GroupDao* group_dao,
                       MessageDao* message_dao,
                       OfflineMessageDao* offline_message_dao,
                       RedisClient* redis_client,
                       KafkaProducer* kafka_producer,
                       MessageIdGenerator* message_id_generator,
                       MessageDeduplicator* message_deduplicator,
                       MessageServiceOptions options,
                       MySqlConnectionPool* mysql_pool = nullptr,
                       ConversationDao* conversation_dao = nullptr,
                       MessageReceiptDao* message_receipt_dao = nullptr,
                       OutboxDao* outbox_dao = nullptr);

    grpc::Status SendSingleMessage(grpc::ServerContext* context,
                                   const proto::SendSingleMessageRequest* request,
                                   proto::SendSingleMessageResponse* response) override;

    grpc::Status SendGroupMessage(grpc::ServerContext* context,
                                  const proto::SendGroupMessageRequest* request,
                                  proto::SendGroupMessageResponse* response) override;

    grpc::Status AckMessage(grpc::ServerContext* context,
                            const proto::AckMessageRequest* request,
                            proto::AckMessageResponse* response) override;

    grpc::Status PullOfflineMessages(grpc::ServerContext* context,
                                     const proto::PullOfflineMessagesRequest* request,
                                     proto::PullOfflineMessagesResponse* response) override;

    grpc::Status MarkMessageRead(grpc::ServerContext* context,
                                 const proto::MarkMessageReadRequest* request,
                                 proto::CommonResponse* response) override;

    grpc::Status MarkConversationRead(grpc::ServerContext* context,
                                      const proto::MarkConversationReadRequest* request,
                                      proto::CommonResponse* response) override;

    grpc::Status GetMessageReadState(grpc::ServerContext* context,
                                     const proto::GetMessageReadStateRequest* request,
                                     proto::GetMessageReadStateResponse* response) override;

    grpc::Status RecallMessage(grpc::ServerContext* context,
                               const proto::RecallMessageRequest* request,
                               proto::RecallMessageResponse* response) override;

private:
    bool invalidDeps() const;
    bool validateContent(const std::string& request_id, const std::string& content, proto::CommonResponse* response) const;

private:
    UserDao* user_dao_;
    GroupDao* group_dao_;
    MessageDao* message_dao_;
    OfflineMessageDao* offline_message_dao_;
    RedisClient* redis_client_;
    KafkaProducer* kafka_producer_;
    MessageIdGenerator* message_id_generator_;
    MessageDeduplicator* message_deduplicator_;
    MySqlConnectionPool* mysql_pool_;
    ConversationDao* conversation_dao_;
    MessageReceiptDao* message_receipt_dao_;
    OutboxDao* outbox_dao_;
    MessageServiceOptions options_;
};

}  // namespace nebula
