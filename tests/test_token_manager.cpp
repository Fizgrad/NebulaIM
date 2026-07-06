#include "common/auth/TokenManager.h"
#include "common/utils/TimeUtil.h"

#include <cassert>
#include <string>

int main() {
    nebula::TokenManager manager(3600);
    std::string t1 = manager.generateToken(10001);
    std::string t2 = manager.generateToken(10001);

    assert(!t1.empty());
    assert(!t2.empty());
    assert(t1 != t2);
    assert(t1.find("10001.") == std::string::npos);
    assert(manager.tokenKey(t1).rfind("nebula:token:", 0) == 0);
    assert(manager.tokenKey(t1) != "nebula:token:" + t1);
    assert(manager.tokenKey(t1).size() == std::string("nebula:token:").size() + 64);
    assert(manager.ttlSeconds() == 3600);
    assert(manager.expireAtMs() > nebula::TimeUtil::nowMs());

    return 0;
}
