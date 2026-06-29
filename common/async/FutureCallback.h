#pragma once

#include <exception>
#include <functional>
#include <utility>

namespace nebula {

template <typename T>
struct FutureResult {
    T value{};
    std::exception_ptr error;

    explicit operator bool() const { return error == nullptr; }
};

using VoidCallback = std::function<void(std::exception_ptr)>;

}  // namespace nebula
