#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace nebula {

class Buffer {
public:
    static constexpr size_t kCheapPrepend = 8;
    static constexpr size_t kInitialSize = 1024;

    Buffer();

    size_t readableBytes() const;
    size_t writableBytes() const;
    size_t prependableBytes() const;

    const char* peek() const;

    void retrieve(size_t len);
    void retrieveAll();

    std::string retrieveAsString(size_t len);
    std::string retrieveAllAsString();

    void append(const std::string& data);
    void append(const char* data, size_t len);

    void ensureWritableBytes(size_t len);

    char* beginWrite();
    const char* beginWrite() const;
    void hasWritten(size_t len);

    ssize_t readFd(int fd, int* saved_errno);
    ssize_t writeFd(int fd, int* saved_errno);

private:
    char* begin();
    const char* begin() const;
    void makeSpace(size_t len);

private:
    std::vector<char> buffer_;
    size_t reader_index_;
    size_t writer_index_;
};

}  // namespace nebula
