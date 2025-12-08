#pragma once
// Copyright (c) 2025, WH, All rights reserved.
#ifndef COLOR_H
#define COLOR_H

#include "types.h"

#include <algorithm>
#include <cstdint>

using Channel = u8;
namespace Colors {
template <typename T>
concept Numeric = std::is_arithmetic_v<T>;

// helper to detect if types are "compatible"
template <typename T, typename U>
inline constexpr bool is_compatible_v =
    std::is_same_v<T, U> ||
    // integer literals
    (std::is_integral_v<T> && std::is_integral_v<U> && (std::is_convertible_v<T, U> || std::is_convertible_v<U, T>)) ||
    // same "family" of types (all floating or all integral)
    (std::is_floating_point_v<T> && std::is_floating_point_v<U>) || (std::is_integral_v<T> && std::is_integral_v<U>);

// check if all four types are compatible with each other
template <typename A, typename R, typename G, typename B>
inline constexpr bool all_compatible_v = is_compatible_v<A, R> && is_compatible_v<A, G> && is_compatible_v<A, B> &&
                                         is_compatible_v<R, G> && is_compatible_v<R, B> && is_compatible_v<G, B>;

constexpr Channel to_byte(Numeric auto value) {
    if constexpr(std::is_floating_point_v<decltype(value)>)
        return static_cast<Channel>(std::clamp<decltype(value)>(value, 0, 1) * 255);
    else
        return static_cast<Channel>(std::clamp<Channel>(value, 0, 255));
}
}  // namespace Colors

// argb colors
struct Color {
    u32 v;

    constexpr Color() : v(0) {}
    constexpr Color(u32 val) : v(val) {}

    constexpr Color(Channel alpha, Channel red, Channel green, Channel blue) {
        v = (static_cast<u32>(alpha) << 24) | (static_cast<u32>(red) << 16) | (static_cast<u32>(green) << 8) |
            static_cast<u32>(blue);
    }

    // clang-format off
	// channel accessors (couldn't make the union work, unfortunate)
	[[nodiscard]] constexpr Channel A() const { return static_cast<Channel>((v >> 24) & 0xFF); }
	[[nodiscard]] constexpr Channel R() const { return static_cast<Channel>((v >> 16) & 0xFF); }
	[[nodiscard]] constexpr Channel G() const { return static_cast<Channel>((v >> 8) & 0xFF); }
	[[nodiscard]] constexpr Channel B() const { return static_cast<Channel>(v & 0xFF); }

	// float accessors (normalized to 0.0-1.0)
	template <typename T = float>
	[[nodiscard]] constexpr T Af() const { return static_cast<T>(static_cast<float>((v >> 24) & 0xFF) / 255.0f); }
	template <typename T = float>
	[[nodiscard]] constexpr T Rf() const { return static_cast<T>(static_cast<float>((v >> 16) & 0xFF) / 255.0f); }
	template <typename T = float>
	[[nodiscard]] constexpr T Gf() const { return static_cast<T>(static_cast<float>((v >> 8) & 0xFF) / 255.0f); }
	template <typename T = float>
	[[nodiscard]] constexpr T Bf() const { return static_cast<T>(static_cast<float>(v & 0xFF) / 255.0f); }

	template <typename T = Channel>
	constexpr Color &setA(T a) { *this = ((*this & 0x00FFFFFF) | (Colors::to_byte(a) << 24)); return *this; }
	template <typename T = Channel>
	constexpr Color &setR(T r) { *this = ((*this & 0xFF00FFFF) | (Colors::to_byte(r) << 16)); return *this; }
	template <typename T = Channel>
	constexpr Color &setG(T g) { *this = ((*this & 0xFFFF00FF) | (Colors::to_byte(g) << 8)); return *this; }
	template <typename T = Channel>
	constexpr Color &setB(T b) { *this = ((*this & 0xFFFFFF00) | (Colors::to_byte(b) << 0)); return *this; }

	constexpr Color& operator&=(u32 val) { v &= val; return *this; }
	constexpr Color& operator|=(u32 val) { v |= val; return *this; }
	constexpr Color& operator^=(u32 val) { v ^= val; return *this; }
	constexpr Color& operator<<=(int shift) { v <<= shift; return *this; }
	constexpr Color& operator>>=(int shift) { v >>= shift; return *this; }

	constexpr Color& operator&=(const Color& other) { v &= other.v; return *this; }
	constexpr Color& operator|=(const Color& other) { v |= other.v; return *this; }
	constexpr Color& operator^=(const Color& other) { v ^= other.v; return *this; }
    // clang-format on

    operator u32() const { return v; }
};

// main conversion func
template <typename A, typename R, typename G, typename B>
constexpr Color argb(A a, R r, G g, B b)
    requires Colors::Numeric<A> && Colors::Numeric<R> && Colors::Numeric<G> && Colors::Numeric<B> &&
             Colors::all_compatible_v<A, R, G, B>
{
    return Color(Colors::to_byte(a), Colors::to_byte(r), Colors::to_byte(g), Colors::to_byte(b));
}

template <typename A, typename R, typename G, typename B>
[[deprecated("parameters should have compatible types")]]
constexpr Color argb(A a, R r, G g, B b)
    requires Colors::Numeric<A> && Colors::Numeric<R> && Colors::Numeric<G> && Colors::Numeric<B> &&
             (!Colors::all_compatible_v<A, R, G, B>)
{
    return Color(Colors::to_byte(a), Colors::to_byte(r), Colors::to_byte(g), Colors::to_byte(b));
}

// convenience
template <typename R, typename G, typename B, typename A>
constexpr Color rgba(R r, G g, B b, A a)
    requires Colors::Numeric<R> && Colors::Numeric<G> && Colors::Numeric<B> && Colors::Numeric<A> &&
             Colors::all_compatible_v<R, G, B, A>
{
    return argb(a, r, g, b);
}

constexpr Color argb(Color rgbacol) { return argb(rgbacol.B(), rgbacol.A(), rgbacol.R(), rgbacol.G()); }

// for opengl
constexpr Color rgba(Color argbcol) { return argb(argbcol.R(), argbcol.G(), argbcol.B(), argbcol.A()); }

// for opengl
constexpr Color abgr(Color argbcol) { return argb(argbcol.A(), argbcol.B(), argbcol.G(), argbcol.R()); }

template <typename R, typename G, typename B, typename A>
[[deprecated("parameters should have compatible types")]]
constexpr Color rgba(R r, G g, B b, A a)
    requires Colors::Numeric<R> && Colors::Numeric<G> && Colors::Numeric<B> && Colors::Numeric<A> &&
             (!Colors::all_compatible_v<R, G, B, A>)
{
    return argb(a, r, g, b);
}

template <typename R, typename G, typename B>
constexpr Color rgb(R r, G g, B b)
    requires Colors::Numeric<R> && Colors::Numeric<G> && Colors::Numeric<B> && Colors::all_compatible_v<R, G, B, R>
{
    return Color(255, Colors::to_byte(r), Colors::to_byte(g), Colors::to_byte(b));
}

template <typename R, typename G, typename B>
[[deprecated("parameters should have compatible types")]]
constexpr Color rgb(R r, G g, B b)
    requires Colors::Numeric<R> && Colors::Numeric<G> && Colors::Numeric<B> && (!Colors::all_compatible_v<R, G, B, R>)
{
    return Color(255, Colors::to_byte(r), Colors::to_byte(g), Colors::to_byte(b));
}

namespace Colors {
constexpr Color brighten(Color color, float factor) {
    return argb(color.Af(), color.Rf() * factor, color.Gf() * factor, color.Bf() * factor);
}

constexpr Color invert(Color color) {
    return {static_cast<Channel>((color.v >> 24) & 0xFF), static_cast<Channel>(255 - ((color.v >> 16) & 0xFF)),
            static_cast<Channel>(255 - ((color.v >> 8) & 0xFF)), static_cast<Channel>(255 - (color.v & 0xFF))};
}

constexpr Color multiply(Color color1, Color color2) {
    return rgb(color1.Rf() * color2.Rf(), color1.Gf() * color2.Gf(), color1.Bf() * color2.Bf());
}

constexpr Color add(Color color1, Color color2) {
    return rgb(std::clamp(color1.Rf() + color2.Rf(), 0.0f, 1.0f), std::clamp(color1.Gf() + color2.Gf(), 0.0f, 1.0f),
               std::clamp(color1.Bf() + color2.Bf(), 0.0f, 1.0f));
}

constexpr Color subtract(Color color1, Color color2) {
    return rgb(std::clamp(color1.Rf() - color2.Rf(), 0.0f, 1.0f), std::clamp(color1.Gf() - color2.Gf(), 0.0f, 1.0f),
               std::clamp(color1.Bf() - color2.Bf(), 0.0f, 1.0f));
}

constexpr Color scale(Color color, float multiplier) {
    return {static_cast<Channel>((color.v >> 24) & 0xFF),
            static_cast<Channel>(static_cast<float>((color.v >> 16) & 0xFF) * multiplier),
            static_cast<Channel>(static_cast<float>((color.v >> 8) & 0xFF) * multiplier),
            static_cast<Channel>(static_cast<float>(color.v & 0xFF) * multiplier)};
}
}  // namespace Colors

#endif
