#include "message.pb.h"

#include <cassert>
#include <iostream>

int main() {
    nebula::proto::RecallMessageRequest req;
    req.set_user_id(1);
    req.set_message_id(2);
    assert(req.user_id() == 1);
    assert(req.message_id() == 2);

    nebula::proto::MessageData data;
    data.set_recalled(true);
    data.set_recalled_at(123);
    assert(data.recalled());
    std::cout << "test_message_recall passed\n";
    return 0;
}
