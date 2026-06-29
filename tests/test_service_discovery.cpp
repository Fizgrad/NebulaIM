#include "common/discovery/StaticServiceResolver.h"

#include <cassert>
#include <iostream>

int main() {
    nebula::StaticServiceResolver resolver;
    resolver.addInstance("user_service", "127.0.0.1:50051", "user-1");
    resolver.addInstance("user_service", "127.0.0.1:150051", "user-2");

    auto all = resolver.resolve("user_service");
    assert(all.size() == 2);
    auto picked = resolver.pick("user_service");
    assert(picked.service_name == "user_service");
    assert(!picked.address.empty());

    std::cout << "test_service_discovery passed\n";
    return 0;
}
