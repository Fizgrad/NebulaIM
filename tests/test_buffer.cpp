#include "common/net/Buffer.h"

#include <cassert>
#include <cerrno>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

int main() {
    nebula::Buffer buffer;
    assert(buffer.readableBytes() == 0);
    assert(buffer.writableBytes() == nebula::Buffer::kInitialSize);

    buffer.append("hello");
    assert(buffer.readableBytes() == 5);
    assert(buffer.retrieveAsString(2) == "he");
    assert(buffer.readableBytes() == 3);

    buffer.append(" world", 6);
    assert(buffer.retrieveAllAsString() == "llo world");
    assert(buffer.readableBytes() == 0);

    std::string large(4096, 'x');
    buffer.append(large);
    assert(buffer.readableBytes() == large.size());
    assert(buffer.retrieveAllAsString() == large);

    buffer.ensureWritableBytes(8192);
    assert(buffer.writableBytes() >= 8192);

    int sockets[2] = {-1, -1};
    assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    ::close(sockets[1]);
    nebula::Buffer disconnected;
    disconnected.append("write after peer close");
    int saved_errno = 0;
    assert(disconnected.writeFd(sockets[0], &saved_errno) == -1);
    assert(saved_errno == EPIPE || saved_errno == ECONNRESET);
    ::close(sockets[0]);

    return 0;
}
