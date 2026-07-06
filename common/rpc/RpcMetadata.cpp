#include "common/rpc/RpcMetadata.h"

#include <grpcpp/grpcpp.h>

namespace nebula {

namespace {
std::string getMetadata(const grpc::ServerContext* context, const char* key) {
    if (context == nullptr) return "";
    auto range = context->client_metadata().equal_range(key);
    if (range.first == range.second) return "";
    return std::string(range.first->second.data(), range.first->second.length());
}
}

void RpcMetadata::injectTraceId(grpc::ClientContext* context, const std::string& trace_id) {
    if (context != nullptr && !trace_id.empty()) context->AddMetadata(kTraceIdKey, trace_id);
}

std::string RpcMetadata::extractTraceId(const grpc::ServerContext* context) {
    return getMetadata(context, kTraceIdKey);
}

void RpcMetadata::injectAdminToken(grpc::ClientContext* context, const std::string& token) {
    if (context != nullptr && !token.empty()) context->AddMetadata(kAdminTokenKey, token);
}

std::string RpcMetadata::extractAdminToken(const grpc::ServerContext* context) {
    return getMetadata(context, kAdminTokenKey);
}

void RpcMetadata::injectInternalToken(grpc::ClientContext* context, const std::string& token) {
    if (context != nullptr && !token.empty()) context->AddMetadata(kInternalTokenKey, token);
}

std::string RpcMetadata::extractInternalToken(const grpc::ServerContext* context) {
    return getMetadata(context, kInternalTokenKey);
}

}  // namespace nebula
