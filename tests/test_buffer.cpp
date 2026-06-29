#include "common/net/Buffer.h"

#include <cassert>
#include <string>

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

    return 0;
}
