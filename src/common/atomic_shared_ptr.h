// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <atomic>
#include <memory>
#include <utility>

namespace Common {

// Publish immutable snapshots without changing their lifetime while readers use them.
// Older Apple libc++ lacks atomic<shared_ptr<T>>; retain its supported free-function
// implementation there. Modern libraries use the C++20 specialization instead.
template <typename T>
class AtomicSharedPtr {
public:
    AtomicSharedPtr() noexcept = default;
    explicit AtomicSharedPtr(std::shared_ptr<T> initial) noexcept : value(std::move(initial)) {}
    AtomicSharedPtr(const AtomicSharedPtr&) = delete;
    AtomicSharedPtr& operator=(const AtomicSharedPtr&) = delete;

    [[nodiscard]] std::shared_ptr<T> load() const noexcept {
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
        return value.load();
#else
        return std::atomic_load(&value);
#endif
    }

    void store(std::shared_ptr<T> next) noexcept {
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
        value.store(std::move(next));
#else
        std::atomic_store(&value, std::move(next));
#endif
    }

    [[nodiscard]] std::shared_ptr<T> exchange(std::shared_ptr<T> next) noexcept {
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
        return value.exchange(std::move(next));
#else
        return std::atomic_exchange(&value, std::move(next));
#endif
    }

private:
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
    std::atomic<std::shared_ptr<T>> value{};
#else
    std::shared_ptr<T> value;
#endif
};

} // namespace Common
