// Copyright (c) 2025, WH, All rights reserved.
#pragma once

#include <atomic>
#include <memory>

// fallback for libc++ missing std::atomic<std::shared_ptr<T>> (STILL! and std::atomic_*(std::shared_ptr<T>) is already deprecated in C++20 ...)
#if __cpp_lib_atomic_shared_ptr >= 201711L
template <typename T>
using mcatomic_shptr = std::atomic<std::shared_ptr<T>>;
#else
template <typename T>
class mcatomic_shptr {
    mutable std::shared_ptr<T> m_ptr;

   public:
    mcatomic_shptr() = default;
    ~mcatomic_shptr() = default;

    explicit mcatomic_shptr(std::shared_ptr<T> desired) : m_ptr(std::move(desired)) {}

    mcatomic_shptr(const mcatomic_shptr&) = delete;
    mcatomic_shptr& operator=(const mcatomic_shptr&) = delete;
    mcatomic_shptr(mcatomic_shptr&&) = delete;
    mcatomic_shptr& operator=(mcatomic_shptr&&) = delete;

    std::shared_ptr<T> load(std::memory_order order = std::memory_order_seq_cst) const {
        return std::atomic_load_explicit(&m_ptr, order);
    }

    void store(std::shared_ptr<T> desired, std::memory_order order = std::memory_order_seq_cst) {
        std::atomic_store_explicit(&m_ptr, std::move(desired), order);
    }

    std::shared_ptr<T> exchange(std::shared_ptr<T> desired, std::memory_order order = std::memory_order_seq_cst) {
        return std::atomic_exchange_explicit(&m_ptr, std::move(desired), order);
    }

    bool compare_exchange_weak(std::shared_ptr<T>& expected, std::shared_ptr<T> desired,
                               std::memory_order order = std::memory_order_seq_cst) {
        return std::atomic_compare_exchange_weak_explicit(&m_ptr, &expected, std::move(desired), order);
    }

    bool compare_exchange_strong(std::shared_ptr<T>& expected, std::shared_ptr<T> desired,
                                 std::memory_order order = std::memory_order_seq_cst) {
        return std::atomic_compare_exchange_strong_explicit(&m_ptr, &expected, std::move(desired), order);
    }

    operator std::shared_ptr<T>() const { return load(); }

    mcatomic_shptr& operator=(std::shared_ptr<T> desired) {
        store(std::move(desired));
        return *this;
    }
};
#endif
