#pragma once

#include <string>

#if defined(NEBULA_ENABLE_GRPC)
#include <grpcpp/grpcpp.h>
#else
namespace grpc {
class ClientContext;
class ServerContext;
}
#endif

namespace nebula {

class RpcMetadata {
public:
    static constexpr const char* kTraceIdKey = "x-nebula-trace-id";
    static constexpr const char* kAdminTokenKey = "x-nebula-admin-token";

    static void injectTraceId(grpc::ClientContext* context, const std::string& trace_id);
    static std::string extractTraceId(const grpc::ServerContext* context);
    static void injectAdminToken(grpc::ClientContext* context, const std::string& token);
    static std::string extractAdminToken(const grpc::ServerContext* context);
};

}  // namespace nebula
