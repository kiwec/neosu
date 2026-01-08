// Copyright (c) 2025, WH, All rights reserved.
#pragma once
#ifndef STATIC_PIMPL_H
#define STATIC_PIMPL_H

#include <cstddef>
#include <cstdlib>
#include <cassert>
#include <new>
#include <type_traits>
#include <utility>

#ifndef forceinline
#if defined(__GNUC__) || defined(__clang__)
#define forceinline __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#define forceinline __forceinline
#else
#define forceinline inline
#endif
#endif

// for non-dynamically-allocated "pImpl" use-cases
// basically, so you don't have to declare things inside of a class which are meaningless outside of its own implementation details
// helps reduce compile time by decreasing the size of public headers and transitive includes, without the downside of
// needing to have dynamically allocated objects scattered around the heap just to support that ("that" being forward-declared class members)
template <typename T, size_t RealImplSize, size_t BufferAlignment = 2 * sizeof(void *)>
class StaticPImpl {
   private:
#if defined(_MSC_VER) && !defined(__clang__)
// a bunch of common stdlib things are annoyingly way bigger on msvc
// so just use a flat 1.25x multiplier (since its not even a "real" platform we support)
#define MSVC_BLOAT_ACCOMODATION_MULTIPLIER 1.25
#else
#define MSVC_BLOAT_ACCOMODATION_MULTIPLIER 1
#endif
#define PMUL_(real_) (size_t)((real_) * MSVC_BLOAT_ACCOMODATION_MULTIPLIER)
    alignas(BufferAlignment) unsigned char m_buffer[PMUL_(RealImplSize)];

    void (*m_destructor)(void *);

   public:
    [[nodiscard]] forceinline T *operator->() noexcept { return std::launder(reinterpret_cast<T *>(m_buffer)); }

    [[nodiscard]] forceinline const T *operator->() const noexcept {
        return std::launder(reinterpret_cast<const T *>(m_buffer));
    }

    [[nodiscard]] forceinline T &operator*() noexcept { return *std::launder(reinterpret_cast<T *>(m_buffer)); }

    [[nodiscard]] forceinline const T &operator*() const noexcept {
        return *std::launder(reinterpret_cast<const T *>(m_buffer));
    }

    // Construct a derived type U in-place (U must be T or derived from T)
    template <typename U, typename... Args>
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
    [[nodiscard]] forceinline explicit StaticPImpl(std::in_place_type_t<U> /**/, Args &&...args)
        : m_destructor([](void *ptr) { static_cast<U *>(ptr)->~U(); }) {
        static_assert(sizeof(U) <= PMUL_(RealImplSize));
        static_assert(alignof(U) <= BufferAlignment);
        static_assert(std::is_same_v<T, U> || std::is_base_of_v<T, U>);

        new(m_buffer) U(std::forward<Args>(args)...);
    }

    template <typename... Args>
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
    [[nodiscard]] forceinline explicit StaticPImpl(Args &&...args)
        : StaticPImpl(std::in_place_type<T>, std::forward<Args>(args)...) {}

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
    [[nodiscard]] forceinline StaticPImpl(const StaticPImpl &rhs)
        : m_destructor([](void *ptr) { static_cast<T *>(ptr)->~T(); }) {
        new(m_buffer) T(*std::launder(reinterpret_cast<const T *>(rhs.m_buffer)));
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
    [[nodiscard]] forceinline StaticPImpl(StaticPImpl &&rhs) noexcept
        : m_destructor([](void *ptr) { static_cast<T *>(ptr)->~T(); }) {
        new(m_buffer) T(static_cast<T &&>(*std::launder(reinterpret_cast<T *>(rhs.m_buffer))));
    }

    // NOLINTNEXTLINE(bugprone-unhandled-self-assignment, cert-oop54-cpp) // let the actual object handle self assignment
    forceinline StaticPImpl &operator=(const StaticPImpl &rhs) {
        *std::launder(reinterpret_cast<T *>(m_buffer)) = *std::launder(reinterpret_cast<const T *>(rhs.m_buffer));
        return *this;
    }

    forceinline StaticPImpl &operator=(StaticPImpl &&rhs) noexcept {
        *std::launder(reinterpret_cast<T *>(m_buffer)) =
            static_cast<T &&>(*std::launder(reinterpret_cast<T *>(rhs.m_buffer)));
        return *this;
    }

    forceinline ~StaticPImpl() {
        if(m_destructor) {
            m_destructor(m_buffer);
        }
        m_destructor = nullptr;
    }
};

#undef PMUL_
#undef MSVC_BLOAT_ACCOMODATION_MULTIPLIER

#endif
