#include "common/dao/FriendRequestDao.h"

#include <cassert>
#include <iostream>

int main() {
    nebula::FriendRequest request;
    request.id = 1;
    request.from_user_id = 100;
    request.to_user_id = 200;
    request.status = static_cast<int>(nebula::FriendRequestStatus::PENDING);
    assert(request.status == 0);
    request.status = static_cast<int>(nebula::FriendRequestStatus::ACCEPTED);
    assert(request.status == 1);
    std::cout << "test_friend_request_flow passed\n";
    return 0;
}
