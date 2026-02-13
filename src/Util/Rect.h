#pragma once
// Copyright (c) 2012, PG, All rights reserved.
#include "noinclude.h"
#include "Vectors.h"

class McRect {
   public:
    constexpr McRect(float x = 0, float y = 0, float width = 0, float height = 0, bool isCentered = false) {
        this->set(x, y, width, height, isCentered);
    }

    constexpr McRect(vec2 pos, vec2 size, bool isCentered = false) { this->set(pos, size, isCentered); }

    // loosely within (inside or equals (+ lenience amount))
    [[nodiscard]] inline bool contains(vec2 point, float lenience = 0.f) const {
        return vec::all(vec::greaterThanEqual(point + lenience, this->vMin)) &&
               vec::all(vec::lessThanEqual(point - lenience, this->vMin + this->vSize));
    }

    // strictly within (not or-equal)
    [[nodiscard]] inline bool containsStrict(vec2 point) const {
        return vec::all(vec::greaterThan(point, this->vMin)) &&
               vec::all(vec::lessThan(point, this->vMin + this->vSize));
    }

    [[nodiscard]] forceinline bool intersects(const McRect &rect) const {
        const vec2 maxMin = vec::max(this->vMin, rect.vMin);
        const vec2 minMax = vec::min(this->vMin + this->vSize, rect.vMin + rect.vSize);
        return maxMin.x < minMax.x && maxMin.y < minMax.y;
    }
    [[nodiscard]] McRect intersect(const McRect &rect) const;

    [[nodiscard]] McRect Union(const McRect &rect) const;

    [[nodiscard]] inline vec2 getCenter() const { return this->vMin + this->vSize * 0.5f; }
    [[nodiscard]] inline vec2 getMax() const { return this->vMin + this->vSize; }

    // get
    [[nodiscard]] constexpr const vec2 &getPos() const { return this->vMin; }
    [[nodiscard]] constexpr const vec2 &getMin() const { return this->vMin; }
    [[nodiscard]] constexpr const vec2 &getSize() const { return this->vSize; }

    [[nodiscard]] constexpr const float &getX() const { return this->vMin.x; }
    [[nodiscard]] constexpr const float &getY() const { return this->vMin.y; }
    [[nodiscard]] constexpr const float &getMinX() const { return this->vMin.x; }
    [[nodiscard]] constexpr const float &getMinY() const { return this->vMin.y; }

    [[nodiscard]] inline float getMaxX() const { return this->vMin.x + this->vSize.x; }
    [[nodiscard]] inline float getMaxY() const { return this->vMin.y + this->vSize.y; }

    [[nodiscard]] constexpr const float &getWidth() const { return this->vSize.x; }
    [[nodiscard]] constexpr const float &getHeight() const { return this->vSize.y; }

    // set
    inline void setMin(vec2 min) { this->vMin = min; }
    inline void setMax(vec2 max) { this->vSize = max - this->vMin; }
    inline void setMinX(float minx) { this->vMin.x = minx; }
    inline void setMinY(float miny) { this->vMin.y = miny; }
    inline void setMaxX(float maxx) { this->vSize.x = maxx - this->vMin.x; }
    inline void setMaxY(float maxy) { this->vSize.y = maxy - this->vMin.y; }
    inline void setPos(vec2 pos) { this->vMin = pos; }
    inline void setPosX(float posx) { this->vMin.x = posx; }
    inline void setPosY(float posy) { this->vMin.y = posy; }
    inline void setSize(vec2 size) { this->vSize = size; }
    inline void setWidth(float width) { this->vSize.x = width; }
    inline void setHeight(float height) { this->vSize.y = height; }

    bool operator==(const McRect &rhs) const { return (this->vMin == rhs.vMin) && (this->vSize == rhs.vSize); }

   private:
    constexpr void set(float x, float y, float width, float height, bool isCentered = false) {
        this->set(vec2(x, y), vec2(width, height), isCentered);
    }

    constexpr void set(vec2 pos, vec2 size, bool isCentered = false) {
        if(isCentered) {
            vec2 halfSize = size * 0.5f;
            this->vMin = pos - halfSize;
        } else {
            this->vMin = pos;
        }
        this->vSize = size;
    }

    vec2 vMin{0.f, 0.f};
    vec2 vSize{0.f, 0.f};

    friend struct fmt::formatter<McRect>;
};

namespace fmt {
template <>
struct formatter<McRect> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) const {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const McRect &r, FormatContext &ctx) const {
        return format_to(ctx.out(), "({:.2f},{:.2f}): {:.2f}x{:.2f}"_cf, r.vMin.x, r.vMin.y, r.vSize.x, r.vSize.y);
    }
};
}  // namespace fmt
