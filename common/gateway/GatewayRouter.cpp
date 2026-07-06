#include "common/gateway/GatewayRouter.h"

#include "common/error/ErrorCode.h"
#include "common/async/RpcExecutor.h"
#include "common/gateway/GatewayPacketHelper.h"
#include "common/log/Logger.h"
#include "common/monitor/MetricsRegistry.h"
#include "common/net/TcpConnection.h"
#include "common/rpc/InternalRpcAuth.h"
#include "common/rpc/RpcMetadata.h"
#include "common/trace/TraceContext.h"
#include "common/trace/TraceSpan.h"
#include "message.pb.h"
#include "user.pb.h"

#include <chrono>
#include <grpcpp/grpcpp.h>
#include <utility>

namespace nebula {

namespace {
void setDeadline(grpc::ClientContext* ctx) {
    ctx->set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(2));
}
}

GatewayRouter::GatewayRouter(ConnectionManager* connection_manager,
                             GatewayOnlineManager* online_manager,
                             GatewayBackendClients* backend_clients,
                             PacketCodec* codec,
                             PacketSender packet_sender,
                             RpcExecutor* rpc_executor)
    : connection_manager_(connection_manager),
      online_manager_(online_manager),
      backend_clients_(backend_clients),
      codec_(codec),
      packet_sender_(std::move(packet_sender)),
      rpc_executor_(rpc_executor) {}

void GatewayRouter::handlePacket(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet) {
    if (connection_manager_) connection_manager_->updateActiveTime(connection_id);
    MetricsRegistry::instance().counter("nebula_gateway_packets_total", "Gateway packets handled").inc();
    switch (packet.type) {
        case MessageType::REGISTER_REQ: handleRegister(conn, connection_id, packet); break;
        case MessageType::LOGIN_REQ: handleLogin(conn, connection_id, packet); break;
        case MessageType::HEARTBEAT_REQ: handleHeartbeat(conn, connection_id, packet); break;
        case MessageType::SEND_SINGLE_MSG_REQ: handleSendSingleMessage(conn, connection_id, packet); break;
        case MessageType::SEND_GROUP_MSG_REQ: handleSendGroupMessage(conn, connection_id, packet); break;
        case MessageType::ACK_REQ: handleAck(conn, connection_id, packet); break;
        case MessageType::PULL_OFFLINE_MSG_REQ: handlePullOfflineMessages(conn, connection_id, packet); break;
        default: sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::GATEWAY_INVALID_PACKET), "unsupported packet", ""); break;
    }
}

void GatewayRouter::setPacketSender(PacketSender packet_sender) {
    packet_sender_ = std::move(packet_sender);
}

void GatewayRouter::setRpcExecutor(RpcExecutor* rpc_executor) {
    rpc_executor_ = rpc_executor;
}

void GatewayRouter::sendPacket(const TcpConnectionPtr& conn, const Packet& packet) {
    if (!conn) return;
    if (packet_sender_) {
        packet_sender_(conn, packet);
        return;
    }
    if (codec_) conn->send(codec_->encode(packet));
}

void GatewayRouter::sendError(const TcpConnectionPtr& conn, uint32_t sequence_id, int code, const std::string& message, const std::string& request_id) {
    LOG_WARN("gateway send error sequence_id=" + std::to_string(sequence_id) +
             " code=" + std::to_string(code) +
             " message=" + message +
             " request_id=" + request_id);
    sendPacket(conn, GatewayPacketHelper::makeErrorResponse(sequence_id, code, message, request_id));
}

bool GatewayRouter::requireAuth(const TcpConnectionPtr& conn, const std::string& connection_id, uint32_t sequence_id, std::optional<ConnectionContext>* context) {
    auto ctx = connection_manager_->getContext(connection_id);
    if (!ctx.has_value() || !ctx->authenticated) {
        sendError(conn, sequence_id, static_cast<int>(ErrorCode::GATEWAY_NOT_AUTHENTICATED), "not authenticated", "");
        return false;
    }
    *context = ctx;
    return true;
}

void GatewayRouter::handleRegister(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet) {
    (void)connection_id;
    proto::RegisterRequest req;
    if (!GatewayPacketHelper::parseBody(packet, &req)) {
        sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::GATEWAY_INVALID_PACKET), "bad register body", "");
        return;
    }
    if (!rate_limiter_.allowLogin(conn ? conn->peerAddress().toIp() : "")) {
        MetricsRegistry::instance().counter("nebula_gateway_rate_limited_total", "Gateway requests rejected by rate limiting").inc();
        sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::RATE_LIMITED), "register rate limited", req.request_id());
        return;
    }
    if (!circuit_breakers_.allowRequest("user_service")) {
        sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::CIRCUIT_OPEN), "user service circuit open", req.request_id());
        return;
    }
    TraceContext::ensureTraceId(req.request_id());
    TraceSpan span("gateway.Register", TraceSpanKind::SERVER);
    span.setAttribute("request_id", req.request_id());
    span.setAttribute("connection_id", connection_id);
    MetricsRegistry::instance().counter("nebula_gateway_register_total", "Gateway register requests").inc();
    if (rpc_executor_ != nullptr) {
        struct RegisterResult {
            proto::RegisterResponse resp;
        };
        uint32_t sequence_id = packet.sequence_id;
        rpc_executor_->submit(conn->getLoop(),
            [this, req]() mutable {
                RegisterResult result;
                grpc::ClientContext ctx;
                setDeadline(&ctx);
                RpcMetadata::injectTraceId(&ctx, TraceContext::ensureTraceId(req.request_id()));
                InternalRpcAuth::instance().inject(&ctx);
                grpc::Status status = backend_clients_->userService()->Register(&ctx, req, &result.resp);
                if (!status.ok()) {
                    circuit_breakers_.recordFailure("user_service");
                    result.resp.mutable_response()->set_code(static_cast<int>(ErrorCode::GATEWAY_BACKEND_RPC_FAILED));
                    result.resp.mutable_response()->set_message(status.error_message());
                    result.resp.mutable_response()->set_request_id(req.request_id());
                } else {
                    circuit_breakers_.recordSuccess("user_service");
                }
                return result;
            },
            [this, connection_id, sequence_id, request_id = req.request_id()](RegisterResult result, std::exception_ptr error) mutable {
                auto live_conn = connection_manager_->getConnection(connection_id);
                if (!live_conn || !live_conn->connected()) return;
                if (error) {
                    sendError(live_conn, sequence_id, static_cast<int>(ErrorCode::SERVICE_UNAVAILABLE), "gateway rpc executor unavailable", request_id);
                    return;
                }
                sendPacket(live_conn, GatewayPacketHelper::makeResponse(MessageType::REGISTER_RESP, sequence_id, result.resp));
            });
        return;
    }
    sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::SERVICE_UNAVAILABLE), "gateway rpc executor unavailable", req.request_id());
}

void GatewayRouter::handleLogin(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet) {
    proto::LoginRequest req;
    if (!GatewayPacketHelper::parseBody(packet, &req)) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::GATEWAY_INVALID_PACKET), "bad login body", ""); return; }
    if (!rate_limiter_.allowLogin(conn ? conn->peerAddress().toIp() : "")) {
        MetricsRegistry::instance().counter("nebula_gateway_rate_limited_total", "Gateway requests rejected by rate limiting").inc();
        sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::RATE_LIMITED), "login rate limited", req.request_id());
        return;
    }
    if (!circuit_breakers_.allowRequest("user_service")) {
        sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::CIRCUIT_OPEN), "user service circuit open", req.request_id());
        return;
    }
    TraceContext::ensureTraceId(req.request_id());
    TraceSpan span("gateway.Login", TraceSpanKind::SERVER);
    span.setAttribute("request_id", req.request_id());
    span.setAttribute("connection_id", connection_id);
    span.setAttribute("device_id", req.device_id().empty() ? connection_id : req.device_id());
    MetricsRegistry::instance().counter("nebula_gateway_login_total", "Gateway login requests").inc();
    if (rpc_executor_ != nullptr) {
        struct LoginResult {
            proto::LoginResponse resp;
            bool rpc_ok = true;
            bool online_ok = true;
            std::string rpc_error;
        };
        uint32_t sequence_id = packet.sequence_id;
        std::string login_device_id = req.device_id().empty() ? connection_id : req.device_id();
        std::string login_platform = req.platform();
        rpc_executor_->submit(conn->getLoop(),
            [this, req, connection_id, login_device_id]() mutable {
                LoginResult result;
                grpc::ClientContext ctx;
                setDeadline(&ctx);
                RpcMetadata::injectTraceId(&ctx, TraceContext::ensureTraceId(req.request_id()));
                InternalRpcAuth::instance().inject(&ctx);
                grpc::Status status = backend_clients_->userService()->Login(&ctx, req, &result.resp);
                if (!status.ok()) {
                    circuit_breakers_.recordFailure("user_service");
                    result.rpc_ok = false;
                    result.rpc_error = status.error_message();
                    result.resp.mutable_response()->set_code(static_cast<int>(ErrorCode::GATEWAY_BACKEND_RPC_FAILED));
                    result.resp.mutable_response()->set_message(status.error_message());
                    result.resp.mutable_response()->set_request_id(req.request_id());
                } else {
                    circuit_breakers_.recordSuccess("user_service");
                }
                if (result.rpc_ok && result.resp.response().code() == 0) {
                    result.online_ok = online_manager_->setOnline(result.resp.user_id(), login_device_id, connection_id);
                    if (!result.online_ok) {
                        result.resp.mutable_response()->set_code(static_cast<int>(ErrorCode::GATEWAY_ONLINE_STATE_FAILED));
                        result.resp.mutable_response()->set_message("online state failed");
                    }
                }
                return result;
            },
            [this, connection_id, sequence_id, request_id = req.request_id(), login_device_id, login_platform](LoginResult result, std::exception_ptr error) mutable {
                auto live_conn = connection_manager_->getConnection(connection_id);
                if (!live_conn || !live_conn->connected()) {
                    if (result.rpc_ok && result.resp.response().code() == 0 && result.online_ok) {
                        online_manager_->setOfflineAsync(result.resp.user_id(), login_device_id, connection_id);
                    }
                    return;
                }
                if (error) {
                    sendError(live_conn, sequence_id, static_cast<int>(ErrorCode::SERVICE_UNAVAILABLE), "gateway rpc executor unavailable", request_id);
                    return;
                }
                if (result.rpc_ok && result.resp.response().code() == 0 && result.online_ok) {
                    connection_manager_->bindUser(connection_id, result.resp.user_id(), result.resp.token(), login_device_id, login_platform);
                }
                sendPacket(live_conn, GatewayPacketHelper::makeResponse(MessageType::LOGIN_RESP, sequence_id, result.resp));
            });
        return;
    }
    sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::SERVICE_UNAVAILABLE), "gateway rpc executor unavailable", req.request_id());
}

void GatewayRouter::handleHeartbeat(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet) {
    auto ctx = connection_manager_->getContext(connection_id);
    if (ctx.has_value() && ctx->authenticated) {
        online_manager_->refreshOnlineAsync(ctx->user_id, ctx->device_id, connection_id);
    }
    proto::CommonResponse resp;
    resp.set_code(0);
    resp.set_message("OK");
    sendPacket(conn, GatewayPacketHelper::makeResponse(MessageType::HEARTBEAT_RESP, packet.sequence_id, resp));
}

void GatewayRouter::handleSendSingleMessage(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet) {
    std::optional<ConnectionContext> auth;
    if (!requireAuth(conn, connection_id, packet.sequence_id, &auth)) return;
    proto::SendSingleMessageRequest req;
    if (!GatewayPacketHelper::parseBody(packet, &req)) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::GATEWAY_INVALID_PACKET), "bad single body", ""); return; }
    if (req.from_user_id() == 0) req.set_from_user_id(auth->user_id);
    if (req.from_user_id() != auth->user_id) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::GATEWAY_PERMISSION_DENIED), "permission denied", req.request_id()); return; }
    if (!rate_limiter_.allowMessage(auth->user_id)) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::RATE_LIMITED), "message rate limited", req.request_id()); return; }
    if (!circuit_breakers_.allowRequest("message_service")) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::CIRCUIT_OPEN), "message service circuit open", req.request_id()); return; }
    TraceContext::ensureTraceId(req.request_id());
    TraceSpan span("gateway.SendSingleMessage", TraceSpanKind::SERVER);
    span.setAttribute("request_id", req.request_id());
    span.setAttribute("connection_id", connection_id);
    span.setAttribute("from_user_id", std::to_string(req.from_user_id()));
    span.setAttribute("to_user_id", std::to_string(req.to_user_id()));
    MetricsRegistry::instance().counter("nebula_gateway_message_send_single_total", "Gateway single-message send requests").inc();
    if (rpc_executor_ != nullptr) {
        struct RpcResult {
            proto::SendSingleMessageResponse resp;
        };
        uint32_t sequence_id = packet.sequence_id;
        rpc_executor_->submit(conn->getLoop(),
            [this, req]() mutable {
                RpcResult result;
                grpc::ClientContext ctx;
                setDeadline(&ctx);
                RpcMetadata::injectTraceId(&ctx, TraceContext::ensureTraceId(req.request_id()));
                InternalRpcAuth::instance().inject(&ctx);
                grpc::Status status = backend_clients_->messageService()->SendSingleMessage(&ctx, req, &result.resp);
                if (!status.ok()) {
                    circuit_breakers_.recordFailure("message_service");
                    result.resp.mutable_response()->set_code(static_cast<int>(ErrorCode::GATEWAY_BACKEND_RPC_FAILED));
                    result.resp.mutable_response()->set_message(status.error_message());
                    result.resp.mutable_response()->set_request_id(req.request_id());
                } else {
                    circuit_breakers_.recordSuccess("message_service");
                }
                return result;
            },
            [this, connection_id, sequence_id, request_id = req.request_id()](RpcResult result, std::exception_ptr error) mutable {
                auto live_conn = connection_manager_->getConnection(connection_id);
                if (!live_conn || !live_conn->connected()) return;
                if (error) {
                    sendError(live_conn, sequence_id, static_cast<int>(ErrorCode::SERVICE_UNAVAILABLE), "gateway rpc executor unavailable", request_id);
                    return;
                }
                sendPacket(live_conn, GatewayPacketHelper::makeResponse(MessageType::SEND_SINGLE_MSG_RESP, sequence_id, result.resp));
            });
        return;
    }
    sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::SERVICE_UNAVAILABLE), "gateway rpc executor unavailable", req.request_id());
}

void GatewayRouter::handleSendGroupMessage(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet) {
    std::optional<ConnectionContext> auth;
    if (!requireAuth(conn, connection_id, packet.sequence_id, &auth)) return;
    proto::SendGroupMessageRequest req;
    if (!GatewayPacketHelper::parseBody(packet, &req)) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::GATEWAY_INVALID_PACKET), "bad group body", ""); return; }
    if (req.from_user_id() == 0) req.set_from_user_id(auth->user_id);
    if (req.from_user_id() != auth->user_id) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::GATEWAY_PERMISSION_DENIED), "permission denied", req.request_id()); return; }
    if (!rate_limiter_.allowMessage(auth->user_id)) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::RATE_LIMITED), "message rate limited", req.request_id()); return; }
    if (!circuit_breakers_.allowRequest("message_service")) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::CIRCUIT_OPEN), "message service circuit open", req.request_id()); return; }
    TraceContext::ensureTraceId(req.request_id());
    TraceSpan span("gateway.SendGroupMessage", TraceSpanKind::SERVER);
    span.setAttribute("request_id", req.request_id());
    span.setAttribute("connection_id", connection_id);
    span.setAttribute("from_user_id", std::to_string(req.from_user_id()));
    span.setAttribute("group_id", std::to_string(req.group_id()));
    MetricsRegistry::instance().counter("nebula_gateway_message_send_group_total", "Gateway group-message send requests").inc();
    if (rpc_executor_ != nullptr) {
        struct RpcResult {
            proto::SendGroupMessageResponse resp;
        };
        uint32_t sequence_id = packet.sequence_id;
        rpc_executor_->submit(conn->getLoop(),
            [this, req]() mutable {
                RpcResult result;
                grpc::ClientContext ctx;
                setDeadline(&ctx);
                RpcMetadata::injectTraceId(&ctx, TraceContext::ensureTraceId(req.request_id()));
                InternalRpcAuth::instance().inject(&ctx);
                grpc::Status status = backend_clients_->messageService()->SendGroupMessage(&ctx, req, &result.resp);
                if (!status.ok()) {
                    circuit_breakers_.recordFailure("message_service");
                    result.resp.mutable_response()->set_code(static_cast<int>(ErrorCode::GATEWAY_BACKEND_RPC_FAILED));
                    result.resp.mutable_response()->set_message(status.error_message());
                    result.resp.mutable_response()->set_request_id(req.request_id());
                } else {
                    circuit_breakers_.recordSuccess("message_service");
                }
                return result;
            },
            [this, connection_id, sequence_id, request_id = req.request_id()](RpcResult result, std::exception_ptr error) mutable {
                auto live_conn = connection_manager_->getConnection(connection_id);
                if (!live_conn || !live_conn->connected()) return;
                if (error) {
                    sendError(live_conn, sequence_id, static_cast<int>(ErrorCode::SERVICE_UNAVAILABLE), "gateway rpc executor unavailable", request_id);
                    return;
                }
                sendPacket(live_conn, GatewayPacketHelper::makeResponse(MessageType::SEND_GROUP_MSG_RESP, sequence_id, result.resp));
            });
        return;
    }
    sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::SERVICE_UNAVAILABLE), "gateway rpc executor unavailable", req.request_id());
}

void GatewayRouter::handleAck(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet) {
    std::optional<ConnectionContext> auth;
    if (!requireAuth(conn, connection_id, packet.sequence_id, &auth)) return;
    proto::AckMessageRequest req;
    if (!GatewayPacketHelper::parseBody(packet, &req)) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::GATEWAY_INVALID_PACKET), "bad ack body", ""); return; }
    if (req.user_id() == 0) req.set_user_id(auth->user_id);
    if (req.user_id() != auth->user_id) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::GATEWAY_PERMISSION_DENIED), "permission denied", req.request_id()); return; }
    if (!circuit_breakers_.allowRequest("message_service")) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::CIRCUIT_OPEN), "message service circuit open", req.request_id()); return; }
    TraceContext::ensureTraceId(req.request_id());
    MetricsRegistry::instance().counter("nebula_gateway_ack_total", "Gateway ack requests").inc();
    if (rpc_executor_ != nullptr) {
        struct RpcResult {
            proto::AckMessageResponse resp;
        };
        uint32_t sequence_id = packet.sequence_id;
        rpc_executor_->submit(conn->getLoop(),
            [this, req]() mutable {
                RpcResult result;
                grpc::ClientContext ctx;
                setDeadline(&ctx);
                RpcMetadata::injectTraceId(&ctx, TraceContext::ensureTraceId(req.request_id()));
                InternalRpcAuth::instance().inject(&ctx);
                grpc::Status status = backend_clients_->messageService()->AckMessage(&ctx, req, &result.resp);
                if (!status.ok()) {
                    circuit_breakers_.recordFailure("message_service");
                    result.resp.mutable_response()->set_code(static_cast<int>(ErrorCode::GATEWAY_BACKEND_RPC_FAILED));
                    result.resp.mutable_response()->set_message(status.error_message());
                    result.resp.mutable_response()->set_request_id(req.request_id());
                } else {
                    circuit_breakers_.recordSuccess("message_service");
                }
                return result;
            },
            [this, connection_id, sequence_id, request_id = req.request_id()](RpcResult result, std::exception_ptr error) mutable {
                auto live_conn = connection_manager_->getConnection(connection_id);
                if (!live_conn || !live_conn->connected()) return;
                if (error) {
                    sendError(live_conn, sequence_id, static_cast<int>(ErrorCode::SERVICE_UNAVAILABLE), "gateway rpc executor unavailable", request_id);
                    return;
                }
                sendPacket(live_conn, GatewayPacketHelper::makeResponse(MessageType::ACK_RESP, sequence_id, result.resp));
            });
        return;
    }
    sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::SERVICE_UNAVAILABLE), "gateway rpc executor unavailable", req.request_id());
}

void GatewayRouter::handlePullOfflineMessages(const TcpConnectionPtr& conn, const std::string& connection_id, const Packet& packet) {
    std::optional<ConnectionContext> auth;
    if (!requireAuth(conn, connection_id, packet.sequence_id, &auth)) return;
    proto::PullOfflineMessagesRequest req;
    if (!GatewayPacketHelper::parseBody(packet, &req)) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::GATEWAY_INVALID_PACKET), "bad pull body", ""); return; }
    if (req.user_id() == 0) req.set_user_id(auth->user_id);
    if (req.user_id() != auth->user_id) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::GATEWAY_PERMISSION_DENIED), "permission denied", req.request_id()); return; }
    if (!circuit_breakers_.allowRequest("message_service")) { sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::CIRCUIT_OPEN), "message service circuit open", req.request_id()); return; }
    TraceContext::ensureTraceId(req.request_id());
    MetricsRegistry::instance().counter("nebula_gateway_pull_offline_total", "Gateway pull-offline requests").inc();
    if (rpc_executor_ != nullptr) {
        struct RpcResult {
            proto::PullOfflineMessagesResponse resp;
        };
        uint32_t sequence_id = packet.sequence_id;
        rpc_executor_->submit(conn->getLoop(),
            [this, req]() mutable {
                RpcResult result;
                grpc::ClientContext ctx;
                setDeadline(&ctx);
                RpcMetadata::injectTraceId(&ctx, TraceContext::ensureTraceId(req.request_id()));
                InternalRpcAuth::instance().inject(&ctx);
                grpc::Status status = backend_clients_->messageService()->PullOfflineMessages(&ctx, req, &result.resp);
                if (!status.ok()) {
                    circuit_breakers_.recordFailure("message_service");
                    result.resp.mutable_response()->set_code(static_cast<int>(ErrorCode::GATEWAY_BACKEND_RPC_FAILED));
                    result.resp.mutable_response()->set_message(status.error_message());
                    result.resp.mutable_response()->set_request_id(req.request_id());
                } else {
                    circuit_breakers_.recordSuccess("message_service");
                }
                return result;
            },
            [this, connection_id, sequence_id, request_id = req.request_id()](RpcResult result, std::exception_ptr error) mutable {
                auto live_conn = connection_manager_->getConnection(connection_id);
                if (!live_conn || !live_conn->connected()) return;
                if (error) {
                    sendError(live_conn, sequence_id, static_cast<int>(ErrorCode::SERVICE_UNAVAILABLE), "gateway rpc executor unavailable", request_id);
                    return;
                }
                sendPacket(live_conn, GatewayPacketHelper::makeResponse(MessageType::PULL_OFFLINE_MSG_RESP, sequence_id, result.resp));
            });
        return;
    }
    sendError(conn, packet.sequence_id, static_cast<int>(ErrorCode::SERVICE_UNAVAILABLE), "gateway rpc executor unavailable", req.request_id());
}

}  // namespace nebula
