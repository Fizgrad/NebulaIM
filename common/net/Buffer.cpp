#include "common/net/Buffer.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <sys/uio.h>
#include <unistd.h>

namespace nebula {

Buffer::Buffer()
    : buffer_(kCheapPrepend + kInitialSize),
      reader_index_(kCheapPrepend),
      writer_index_(kCheapPrepend) {}

size_t Buffer::readableBytes() const {
    return writer_index_ - reader_index_;
}

size_t Buffer::writableBytes() const {
    return buffer_.size() - writer_index_;
}

size_t Buffer::prependableBytes() const {
    return reader_index_;
}

const char* Buffer::peek() const {
    return begin() + reader_index_;
}

void Buffer::retrieve(size_t len) {
    assert(len <= readableBytes());
    if (len < readableBytes()) {
        reader_index_ += len;
    } else {
        retrieveAll();
    }
}

void Buffer::retrieveAll() {
    reader_index_ = kCheapPrepend;
    writer_index_ = kCheapPrepend;
}

std::string Buffer::retrieveAsString(size_t len) {
    assert(len <= readableBytes());
    std::string result(peek(), len);
    retrieve(len);
    return result;
}

std::string Buffer::retrieveAllAsString() {
    return retrieveAsString(readableBytes());
}

void Buffer::append(const std::string& data) {
    append(data.data(), data.size());
}

void Buffer::append(const char* data, size_t len) {
    ensureWritableBytes(len);
    std::copy(data, data + len, beginWrite());
    hasWritten(len);
}

void Buffer::ensureWritableBytes(size_t len) {
    if (writableBytes() < len) {
        makeSpace(len);
    }
    assert(writableBytes() >= len);
}

char* Buffer::beginWrite() {
    return begin() + writer_index_;
}

const char* Buffer::beginWrite() const {
    return begin() + writer_index_;
}

void Buffer::hasWritten(size_t len) {
    assert(len <= writableBytes());
    writer_index_ += len;
}

ssize_t Buffer::readFd(int fd, int* saved_errno) {
    char extra_buffer[65536];
    struct iovec vec[2];
    const size_t writable = writableBytes();

    vec[0].iov_base = begin() + writer_index_;
    vec[0].iov_len = writable;
    vec[1].iov_base = extra_buffer;
    vec[1].iov_len = sizeof(extra_buffer);

    const int iovcnt = writable < sizeof(extra_buffer) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0) {
        if (saved_errno != nullptr) {
            *saved_errno = errno;
        }
    } else if (static_cast<size_t>(n) <= writable) {
        writer_index_ += static_cast<size_t>(n);
    } else {
        writer_index_ = buffer_.size();
        append(extra_buffer, static_cast<size_t>(n) - writable);
    }
    return n;
}

ssize_t Buffer::writeFd(int fd, int* saved_errno) {
    const ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0) {
        if (saved_errno != nullptr) {
            *saved_errno = errno;
        }
        return n;
    }
    retrieve(static_cast<size_t>(n));
    return n;
}

char* Buffer::begin() {
    return buffer_.data();
}

const char* Buffer::begin() const {
    return buffer_.data();
}

void Buffer::makeSpace(size_t len) {
    if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
        buffer_.resize(writer_index_ + len);
        return;
    }

    const size_t readable = readableBytes();
    std::copy(begin() + reader_index_, begin() + writer_index_, begin() + kCheapPrepend);
    reader_index_ = kCheapPrepend;
    writer_index_ = reader_index_ + readable;
}

}  // namespace nebula
