#include "message.pb.h"

#include <cassert>
#include <iostream>

int main() {
    nebula::proto::MessageReadState state;
    state.set_message_id(10);
    state.set_user_id(20);
    state.set_delivered_at(30);
    state.set_read_at(40);
    assert(state.read_at() >= state.delivered_at());
    std::cout << "test_read_receipt passed\n";
    return 0;
}
