#pragma once
// Copyright (c) 2023, kiwec & 2025, WH, All rights reserved.

#include "noinclude.h"

// miscellaneous templates

#include <cstdlib>
#include <string>
#include <string_view>
#include <unordered_map>
#include <cassert>

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

// transparent hash and equality for heterogeneous lookup
struct StringHash {
    using is_transparent = void;

    std::size_t operator()(std::string_view sv) const { return std::hash<std::string_view>{}(sv); }
    std::size_t operator()(const std::string &s) const { return std::hash<std::string>{}(s); }
    std::size_t operator()(const char *s) const { return std::hash<std::string_view>{}(std::string_view(s)); }
};

template <typename T>
using sv_unordered_map = std::unordered_map<std::string, T, StringHash, std::equal_to<>>;
