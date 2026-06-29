#include "common/net/EventLoop.h"

#include <cassert>

int main() {
    nebula::EventLoop loop;
    int once_count = 0;
    int repeat_count = 0;

    loop.runAfter(50, [&]() {
        ++once_count;
    });

    loop.runEvery(30, [&]() {
        ++repeat_count;
        if (repeat_count >= 3) {
            loop.quit();
        }
    });

    loop.loop();
    assert(once_count == 1);
    assert(repeat_count >= 3);
    return 0;
}
