#pragma once
// Copyright (c) 2023, kiwec & 2025, WH, All rights reserved.

#include "noinclude.h"

// miscellaneous templates
#include <algorithm>
#include <memory>
#include <cstdlib>
#include <string>
#include <string_view>
#include <unordered_map>
#include <cassert>
#include <vector>

// zero-initialized dynamic array, similar to std::vector but way faster when you don't need constructors
// obviously don't use it on complex types :)
template <class T>
struct zarray {
    zarray(size_t nb_initial = 0) {
        if(nb_initial > 0) {
            this->reserve(nb_initial);
            this->nb = nb_initial;
        }
    }
    ~zarray() { free(this->memory); }

    void push_back(T t) {
        if(this->nb + 1 > this->max) {
            this->reserve(this->max + this->max / 2 + 1);
        }

        this->memory[this->nb] = t;
        this->nb++;
    }

    void reserve(size_t new_max) {
        if(new_max <= this->max) {
            return;
        }

        if(this->max == 0) {
            this->memory = (T *)calloc(new_max, sizeof(T));
        } else {
            this->memory = (T *)realloc(this->memory, new_max * sizeof(T));
            memset(&this->memory[this->max], 0, (new_max - this->max) * sizeof(T));
        }

        this->max = new_max;
    }

    void resize(size_t new_nb) {
        if(new_nb < this->nb) {
            memset(&this->memory[new_nb], 0, (this->nb - new_nb) * sizeof(T));
        } else if(new_nb > this->max) {
            this->reserve(new_nb);
        }

        this->nb = new_nb;
    }

    void swap(zarray<T> &other) {
        size_t omax = this->max;
        size_t onb = this->nb;
        T *omemory = this->memory;

        this->max = other.max;
        this->nb = other.nb;
        this->memory = other.memory;

        other.max = omax;
        other.nb = onb;
        other.memory = omemory;
    }

    T &operator[](size_t index) const { return this->memory[index]; }
    void clear() { this->nb = 0; }
    T *begin() const { return this->memory; }
    T *data() { return this->memory; }
    [[nodiscard]] bool empty() const { return this->nb == 0; }
    T *end() const { return &this->memory[this->nb]; }
    [[nodiscard]] size_t size() const { return this->nb; }

    inline T &front() noexcept {
        assert(!this->empty());
        return operator[](0);
    }
    inline T &back() noexcept {
        assert(!this->empty());
        return operator[](this->size() - 1);
    }

    inline constexpr const T &front() const noexcept {
        assert(!this->empty());
        return operator[](0);
    }
    inline constexpr const T &back() const noexcept {
        assert(!this->empty());
        return operator[](this->size() - 1);
    }

   private:
    size_t max = 0;
    size_t nb = 0;
    T *memory = NULL;
};

// "fixed-size std::vector", but 2*sizeof(size_t) bytes instead of 3*sizeof(size_t)
template <typename T>
struct FixedSizeArray {
    struct zero_init_t {};
    static inline constexpr zero_init_t zero_init{};

    FixedSizeArray() = default;
    ~FixedSizeArray() = default;

    explicit FixedSizeArray(size_t size) : data_(std::make_unique_for_overwrite<T[]>(size)), size_(size) {}
    FixedSizeArray(size_t size, zero_init_t /**/) : data_(std::make_unique<T[]>(size)), size_(size) {}

    FixedSizeArray(const FixedSizeArray &other)
        : data_(other.size_ ? std::make_unique_for_overwrite<T[]>(other.size_) : nullptr), size_(other.size_) {
        if constexpr(std::is_trivially_copyable_v<T>) {
            if(size_) memcpy(data_.get(), other.data_.get(), size_ * sizeof(T));
        } else {
            std::copy_n(other.data_.get(), size_, data_.get());
        }
    }

    FixedSizeArray &operator=(const FixedSizeArray &other) {
        if(this != &other) {
            size_ = other.size_;
            data_ = other.size_ ? std::make_unique_for_overwrite<T[]>(other.size_) : nullptr;
            if constexpr(std::is_trivially_copyable_v<T>) {
                if(size_) memcpy(data_.get(), other.data_.get(), size_ * sizeof(T));
            } else {
                std::copy_n(other.data_.get(), size_, data_.get());
            }
        }
        return *this;
    }

    FixedSizeArray(FixedSizeArray &&other) noexcept : data_(std::move(other.data_)), size_(other.size_) {
        other.size_ = 0;
    }

    FixedSizeArray &operator=(FixedSizeArray &&other) noexcept {
        if(this != &other) {
            data_ = std::move(other.data_);
            size_ = other.size_;
            other.size_ = 0;
        }
        return *this;
    }

    // constructors/assignment operators from a std::vector
    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
    explicit FixedSizeArray(std::vector<T> &&vec) noexcept
        : data_(vec.size() ? std::make_unique_for_overwrite<T[]>(vec.size()) : nullptr), size_(vec.size()) {
        if constexpr(std::is_trivially_copyable_v<T>) {
            if(size_) memcpy(data_.get(), vec.data(), size_ * sizeof(T));
        } else {
            std::ranges::move(vec, data_.get());
        }
    }

    explicit FixedSizeArray(const std::vector<T> &vec) noexcept
        : data_(vec.size() ? std::make_unique_for_overwrite<T[]>(vec.size()) : nullptr), size_(vec.size()) {
        if constexpr(std::is_trivially_copyable_v<T>) {
            if(size_) memcpy(data_.get(), vec.data(), size_ * sizeof(T));
        } else {
            std::ranges::copy(vec, data_.get());
        }
    }

    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
    FixedSizeArray &operator=(std::vector<T> &&vec) noexcept {
        size_ = vec.size();
        data_ = size_ ? std::make_unique_for_overwrite<T[]>(size_) : nullptr;
        if constexpr(std::is_trivially_copyable_v<T>) {
            if(size_) memcpy(data_.get(), vec.data(), size_ * sizeof(T));
        } else {
            std::ranges::move(vec, data_.get());
        }
        return *this;
    }

    FixedSizeArray &operator=(const std::vector<T> &vec) noexcept {
        size_ = vec.size();
        data_ = size_ ? std::make_unique_for_overwrite<T[]>(size_) : nullptr;
        if constexpr(std::is_trivially_copyable_v<T>) {
            if(size_) memcpy(data_.get(), vec.data(), size_ * sizeof(T));
        } else {
            std::ranges::copy(vec, data_.get());
        }
        return *this;
    }

    [[nodiscard]] constexpr inline T *data() noexcept { return data_.get(); }
    [[nodiscard]] constexpr inline const T *data() const noexcept { return data_.get(); }

    [[nodiscard]] constexpr inline T &operator[](size_t i) noexcept {
        assert(!!data_ && "T &operator[](size_t i): !data_");
        assert(i >= 0 && "T &operator[](size_t i): i < 0");
        assert(i < size_ && "T &operator[](size_t i): i >= size_");
        return data_[i];
    }
    [[nodiscard]] constexpr inline const T &operator[](size_t i) const noexcept {
        assert(!!data_ && "const T &operator[](size_t i) const: !data_");
        assert(i >= 0 && "const T &operator[](size_t i) const: i < 0");
        assert(i < size_ && "const T &operator[](size_t i) const: i >= size_");
        return data_[i];
    }

    [[nodiscard]] constexpr inline T &front() noexcept {
        assert(!empty());
        return operator[](0);
    }

    [[nodiscard]] constexpr inline T &back() noexcept {
        assert(!empty());
        return operator[](size_ - 1);
    }

    [[nodiscard]] inline constexpr const T &front() const noexcept {
        assert(!empty());
        return operator[](0);
    }

    [[nodiscard]] inline constexpr const T &back() const noexcept {
        assert(!empty());
        return operator[](size_ - 1);
    }

    [[nodiscard]] constexpr inline size_t size() const noexcept { return size_; }
    [[nodiscard]] constexpr inline bool empty() const noexcept { return size_ == 0; }

    [[nodiscard]] constexpr inline T *begin() noexcept { return data_.get(); }
    [[nodiscard]] constexpr inline T *end() noexcept { return data_.get() + size_; }
    [[nodiscard]] constexpr inline const T *begin() const noexcept { return data_.get(); }
    [[nodiscard]] constexpr inline const T *end() const noexcept { return data_.get() + size_; }

    [[nodiscard]] constexpr inline const T *cbegin() const noexcept { return data_.get(); }
    [[nodiscard]] constexpr inline const T *cend() const noexcept { return data_.get() + size_; }

    void clear() noexcept {
        size_ = 0;
        data_.reset();
    }

   private:
    std::unique_ptr<T[]> data_{nullptr};
    size_t size_{0};
};

template <typename T, size_t RealImplSize>
class StaticPImpl {
   private:
    alignas(alignof(std::max_align_t)) unsigned char m_buffer[RealImplSize];
    void (*m_destructor)(void *);

   public:
    [[nodiscard]] inline T *operator->() noexcept { return std::launder(reinterpret_cast<T *>(m_buffer)); }

    [[nodiscard]] inline const T *operator->() const noexcept {
        return std::launder(reinterpret_cast<const T *>(m_buffer));
    }

    [[nodiscard]] inline T &operator*() noexcept { return *std::launder(reinterpret_cast<T *>(m_buffer)); }

    [[nodiscard]] inline const T &operator*() const noexcept {
        return *std::launder(reinterpret_cast<const T *>(m_buffer));
    }

    // Construct a derived type U in-place (U must be T or derived from T)
    template <typename U, typename... Args>
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
    [[nodiscard]] inline explicit StaticPImpl(std::in_place_type_t<U> /**/, Args &&...args)
        : m_destructor([](void *ptr) { static_cast<U *>(ptr)->~U(); }) {
        static_assert(sizeof(U) <= RealImplSize);
        static_assert(alignof(U) <= alignof(std::max_align_t));
        static_assert(std::is_same_v<T, U> || std::is_base_of_v<T, U>);

        new(m_buffer) U(std::forward<Args>(args)...);
    }

    template <typename... Args>
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
    [[nodiscard]] inline explicit StaticPImpl(Args &&...args)
        : StaticPImpl(std::in_place_type<T>, std::forward<Args>(args)...) {}

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
    [[nodiscard]] inline StaticPImpl(const StaticPImpl &rhs)
        : m_destructor([](void *ptr) { static_cast<T *>(ptr)->~T(); }) {
        new(m_buffer) T(*std::launder(reinterpret_cast<const T *>(rhs.m_buffer)));
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
    [[nodiscard]] inline StaticPImpl(StaticPImpl &&rhs) noexcept
        : m_destructor([](void *ptr) { static_cast<T *>(ptr)->~T(); }) {
        new(m_buffer) T(static_cast<T &&>(*std::launder(reinterpret_cast<T *>(rhs.m_buffer))));
    }

    // NOLINTNEXTLINE(bugprone-unhandled-self-assignment, cert-oop54-cpp) // let the actual object handle self assignment
    inline StaticPImpl &operator=(const StaticPImpl &rhs) {
        *std::launder(reinterpret_cast<T *>(m_buffer)) = *std::launder(reinterpret_cast<const T *>(rhs.m_buffer));
        return *this;
    }

    inline StaticPImpl &operator=(StaticPImpl &&rhs) noexcept {
        *std::launder(reinterpret_cast<T *>(m_buffer)) =
            static_cast<T &&>(*std::launder(reinterpret_cast<T *>(rhs.m_buffer)));
        return *this;
    }

    inline ~StaticPImpl() { m_destructor(m_buffer); }
};

// transparent hash and equality for heterogeneous lookup
struct StringHash {
    using is_transparent = void;

    std::size_t operator()(std::string_view sv) const { return std::hash<std::string_view>{}(sv); }
    std::size_t operator()(const std::string &s) const { return std::hash<std::string>{}(s); }
    std::size_t operator()(const char *s) const { return std::hash<std::string_view>{}(std::string_view(s)); }
};

struct StringEqual {
    using is_transparent = void;

    bool operator()(std::string_view lhs, std::string_view rhs) const { return lhs == rhs; }
    bool operator()(const std::string &lhs, std::string_view rhs) const { return lhs == rhs; }
    bool operator()(std::string_view lhs, const std::string &rhs) const { return lhs == rhs; }
    bool operator()(const std::string &lhs, const std::string &rhs) const { return lhs == rhs; }
};

template <typename T>
using sv_unordered_map = std::unordered_map<std::string, T, StringHash, StringEqual>;
