#include "common/outbox/OutboxWorker.h"

#include <cassert>
#include <iostream>

int main() {
    nebula::OutboxWorker worker(nullptr, nullptr);
    assert(!worker.running());
    worker.runOnce();
    std::cout << "test_outbox_worker passed\n";
    return 0;
}
