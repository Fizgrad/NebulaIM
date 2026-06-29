#include "common/auth/PasswordHasher.h"

#include <cassert>

int main() {
    std::string h1 = nebula::PasswordHasher::hashPassword("password123");
    std::string h2 = nebula::PasswordHasher::hashPassword("password123");

    assert(!h1.empty());
    assert(!h2.empty());
    assert(h1 != h2);
    assert(nebula::PasswordHasher::verifyPassword("password123", h1));
    assert(!nebula::PasswordHasher::verifyPassword("wrong-password", h1));
    assert(nebula::PasswordHasher::hashPassword("").empty());
    assert(!nebula::PasswordHasher::verifyPassword("password123", "bad-format"));

    return 0;
}
