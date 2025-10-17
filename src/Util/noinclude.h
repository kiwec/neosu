// Copyright (c) 2025, WH, All rights reserved.
// miscellaneous utilities/macros which don't require transitive includes
#pragma once

#define SAFE_DELETE(p)  \
    {                   \
        if(p) {         \
            delete(p);  \
            (p) = NULL; \
        }               \
    }

#define PI 3.1415926535897932384626433832795
#define PIOVER180 0.01745329251994329576923690768489

inline bool isInt(float f) { return (f == static_cast<float>(static_cast<int>(f))); }

// not copy or move constructable/assignable
// purely for clarifying intent
#define NOCOPY_NOMOVE(classname__)                       \
   private:                                              \
    classname__(const classname__&) = delete;            \
    classname__& operator=(const classname__&) = delete; \
    classname__(classname__&&) = delete;                 \
    classname__& operator=(classname__&&) = delete;

// create string view literal
#define MC_SV(string__) \
    std::string_view_literals::operator""sv(string__, (sizeof(string__) / sizeof((string__)[0]) - 1))

#if defined(__GNUC__) || defined(__clang__)
#define likely(x) __builtin_expect(bool(x), 1)
#define unlikely(x) __builtin_expect(bool(x), 0)
#define forceinline __attribute__((always_inline)) inline

// force all functions in the function body to be inlined into it
// different from "forceinline", because the function itself won't necessarily be inlined at all call sites
#define INLINE_BODY __attribute__((flatten))

#else
#define likely(x) (x)
#define unlikely(x) (x)
#ifdef _MSC_VER
#define forceinline __forceinline
#else
#define forceinline
#endif
#define INLINE_BODY
#endif