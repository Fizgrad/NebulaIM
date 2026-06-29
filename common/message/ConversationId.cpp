#include "common/message/ConversationId.h"

#include <algorithm>
#include <string>

namespace nebula {

namespace {
uint64_t fnv1a64(const std::string& input) {
    uint64_t hash = 1469598103934665603ULL;
    for (unsigned char c : input) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    return hash;
}
}

uint64_t ConversationId::single(uint64_t user_a, uint64_t user_b) {
    uint64_t a = std::min(user_a, user_b);
    uint64_t b = std::max(user_a, user_b);
    return fnv1a64("single:" + std::to_string(a) + ":" + std::to_string(b));
}

uint64_t ConversationId::group(uint64_t group_id) {
    return fnv1a64("group:" + std::to_string(group_id));
}

}  // namespace nebula
