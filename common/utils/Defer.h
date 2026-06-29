#pragma once

#include <functional>
#include <utility>

namespace nebula {

class Defer {
public:
    explicit Defer(std::function<void()> fn)
        : fn_(std::move(fn)),
          active_(true) {}

    ~Defer() {
        if (active_ && fn_) {
            fn_();
        }
    }

    Defer(const Defer&) = delete;
    Defer& operator=(const Defer&) = delete;

    Defer(Defer&& other) noexcept
        : fn_(std::move(other.fn_)),
          active_(other.active_) {
        other.active_ = false;
    }

    Defer& operator=(Defer&& other) noexcept {
        if (this != &other) {
            if (active_ && fn_) {
                fn_();
            }
            fn_ = std::move(other.fn_);
            active_ = other.active_;
            other.active_ = false;
        }
        return *this;
    }

    void cancel() {
        active_ = false;
    }

private:
    std::function<void()> fn_;
    bool active_;
};

}  // namespace nebula
