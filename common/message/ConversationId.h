#pragma once

#include <cstdint>

namespace nebula {

class ConversationId {
public:
    static uint64_t single(uint64_t user_a, uint64_t user_b);
    static uint64_t group(uint64_t group_id);
};

}  // namespace nebula
