#include "common/utils/TimeUtil.h"

#include <cassert>
#include <iostream>

int main() {
    const int64_t ms = nebula::TimeUtil::nowMs();
    const int64_t us = nebula::TimeUtil::nowUs();
    const std::string text = nebula::TimeUtil::nowString();

    assert(ms > 0);
    assert(us >= ms * 1000);
    assert(text.size() == 23);

    std::cout << text << std::endl;
    return 0;
}
