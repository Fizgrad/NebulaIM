#pragma once

#include <memory>
#include <string>

namespace grpc {
class Channel;
}  // namespace grpc

namespace nebula {

class GrpcClient {
public:
    static std::shared_ptr<grpc::Channel> createInsecureChannel(const std::string& address);
};

}  // namespace nebula
