#include "common/net/EventLoop.h"

#include <cassert>

int main() {
    nebula::EventLoop loop;
    bool called = false;
    loop.runAfter(100, [&]() {
        called = true;
        loop.quit();
    });
    loop.loop();
    assert(called);
    return 0;
}
