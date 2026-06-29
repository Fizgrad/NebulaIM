#include "AdminServiceImpl.h"

#include <cassert>
#include <grpcpp/grpcpp.h>
#include <iostream>

int main() {
    nebula::AdminAuth auth;
    auth.addSha256Token("health-user", "4e598f5daafc2fda61641ddbb5956deb23fde6616366dc9dd5a7c9f47da4d787", {"health"});
    auth.addSha256Token("cleanup-user", "64bd36a04f7f5f7fe6fdb06c79baa6a1e47f9ae7d9ba867b0483d778af0f3881", {"cleanup"});
    auth.addSha256Token("admin-user", "7d16ede5b02f503797228451a0a82e8c78ff0edabe098730d36004964aab3550", {"*"});

    assert(auth.authorize("hashed-secret", "health").decision == nebula::AdminAuthDecision::ALLOW);
    assert(auth.authorize("hashed-secret", "cleanup").decision == nebula::AdminAuthDecision::PERMISSION_DENIED);
    assert(auth.authorize("nebula-admin-local", "cleanup").decision == nebula::AdminAuthDecision::ALLOW);
    assert(auth.authorize("wrong", "health").decision == nebula::AdminAuthDecision::UNAUTHENTICATED);

    nebula::AdminServiceImpl service(auth);
    grpc::ServerContext context;

    nebula::proto::HealthCheckRequest bad_req;
    bad_req.set_request_id("bad");
    nebula::proto::HealthCheckResponse bad_resp;
    assert(service.HealthCheck(&context, &bad_req, &bad_resp).ok());
    assert(bad_resp.response().code() != 0);

    nebula::proto::RunCleanupRequest denied_cleanup_req;
    denied_cleanup_req.set_request_id("denied-cleanup");
    denied_cleanup_req.set_dry_run(true);
    nebula::proto::RunCleanupResponse denied_cleanup_resp;
    assert(service.RunCleanup(&context, &denied_cleanup_req, &denied_cleanup_resp).ok());
    assert(denied_cleanup_resp.response().code() != 0);

    std::cout << "test_admin_auth passed\n";
    return 0;
}
