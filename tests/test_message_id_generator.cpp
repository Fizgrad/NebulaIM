#include "common/message/MessageIdGenerator.h"

#include <cassert>
#include <set>
#include <thread>
#include <vector>

int main() {
    nebula::MessageIdGenerator gen(1);
    std::set<uint64_t> ids;
    uint64_t last = 0;
    for (int i = 0; i < 10000; ++i) {
        uint64_t id = gen.nextId();
        assert(ids.insert(id).second);
        assert(id > last);
        last = id;
    }

    nebula::MessageIdGenerator gen2(2);
    std::set<uint64_t> mt_ids;
    std::mutex mutex;
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 1000; ++i) {
                uint64_t id = gen2.nextId();
                std::lock_guard<std::mutex> lock(mutex);
                assert(mt_ids.insert(id).second);
            }
        });
    }
    for (auto& t : threads) t.join();
    assert(mt_ids.size() == 4000);

    nebula::MessageIdGenerator clipped(9999);
    assert(clipped.nextId() > 0);
    return 0;
}
