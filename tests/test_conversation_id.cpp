#include "common/message/ConversationId.h"

#include <cassert>

int main() {
    assert(nebula::ConversationId::single(1, 2) == nebula::ConversationId::single(2, 1));
    assert(nebula::ConversationId::single(1, 2) != nebula::ConversationId::single(1, 3));
    assert(nebula::ConversationId::group(100) != nebula::ConversationId::single(1, 100));
    assert(nebula::ConversationId::group(100) == nebula::ConversationId::group(100));
    assert(nebula::ConversationId::group(100) != nebula::ConversationId::group(101));
    return 0;
}
