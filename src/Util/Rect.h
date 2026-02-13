#pragma once
// Copyright (c) 2012, PG, All rights reserved.
#include "noinclude.h"
#include "Vectors.h"

template <typename Vec = vec2>
class McRectBase {
    using scalar = typename Vec::value_type;

   public:
    constexpr McRectBase(scalar x = 0, scalar y = 0, scalar width = 0, scalar height = 0, bool isCentered = false) {
        this->set(x, y, width, height, isCentered);
    }

    constexpr McRectBase(Vec pos, Vec size, bool isCentered = false) { this->set(pos, size, isCentered); }

    template <typename OtherVec>
        requires(!std::is_same_v<OtherVec, Vec>)
    constexpr McRectBase(const McRectBase<OtherVec> &other) : vMin(other.vMin), vSize(other.vSize) {}

    // loosely within (inside or equals (+ lenience amount))
    [[nodiscard]] inline bool contains(Vec point, scalar lenience = 0) const {
        return vec::all(vec::greaterThanEqual(point + lenience, this->vMin)) &&
               vec::all(vec::lessThanEqual(point - lenience, this->vMin + this->vSize));
    }

    // strictly within (not or-equal)
    [[nodiscard]] inline bool containsStrict(Vec point) const {
        return vec::all(vec::greaterThan(point, this->vMin)) &&
               vec::all(vec::lessThan(point, this->vMin + this->vSize));
    }

    [[nodiscard]] forceinline bool intersects(const McRectBase &rect) const {
        const Vec maxMin = vec::max(this->vMin, rect.vMin);
        const Vec minMax = vec::min(this->vMin + this->vSize, rect.vMin + rect.vSize);
        return maxMin.x < minMax.x && maxMin.y < minMax.y;
    }

    [[nodiscard]] McRectBase intersect(const McRectBase &rect) const {
        McRectBase intersection;

        Vec thisMax = this->vMin + this->vSize;
        Vec rectMax = rect.vMin + rect.vSize;

        intersection.vMin = vec::max(this->vMin, rect.vMin);
        Vec intersectMax = vec::min(thisMax, rectMax);

        if(vec::any(vec::greaterThan(intersection.vMin, intersectMax))) {
            intersection.vMin = Vec{0};
            intersection.vSize = Vec{0};
        } else {
            intersection.vSize = intersectMax - intersection.vMin;
        }

        return intersection;
    }

    [[nodiscard]] McRectBase Union(const McRectBase &other) const {
        const Vec vMin = vec::min(this->vMin, other.vMin);
        return {vMin, {vec::max(this->getMax(), other.getMax()) - vMin}};
    }

    [[nodiscard]] inline Vec getCenter() const { return this->vMin + this->vSize / scalar(2); }
    [[nodiscard]] inline Vec getMax() const { return this->vMin + this->vSize; }

    // get
    [[nodiscard]] constexpr const Vec &getPos() const { return this->vMin; }
    [[nodiscard]] constexpr const Vec &getMin() const { return this->vMin; }
    [[nodiscard]] constexpr const Vec &getSize() const { return this->vSize; }

    [[nodiscard]] constexpr const scalar &getX() const { return this->vMin.x; }
    [[nodiscard]] constexpr const scalar &getY() const { return this->vMin.y; }
    [[nodiscard]] constexpr const scalar &getMinX() const { return this->vMin.x; }
    [[nodiscard]] constexpr const scalar &getMinY() const { return this->vMin.y; }

    [[nodiscard]] inline scalar getMaxX() const { return this->vMin.x + this->vSize.x; }
    [[nodiscard]] inline scalar getMaxY() const { return this->vMin.y + this->vSize.y; }

    [[nodiscard]] constexpr const scalar &getWidth() const { return this->vSize.x; }
    [[nodiscard]] constexpr const scalar &getHeight() const { return this->vSize.y; }

    // set
    inline void setMin(Vec min) { this->vMin = min; }
    inline void setMax(Vec max) { this->vSize = max - this->vMin; }
    inline void setMinX(scalar minx) { this->vMin.x = minx; }
    inline void setMinY(scalar miny) { this->vMin.y = miny; }
    inline void setMaxX(scalar maxx) { this->vSize.x = maxx - this->vMin.x; }
    inline void setMaxY(scalar maxy) { this->vSize.y = maxy - this->vMin.y; }
    inline void setPos(Vec pos) { this->vMin = pos; }
    inline void setPosX(scalar posx) { this->vMin.x = posx; }
    inline void setPosY(scalar posy) { this->vMin.y = posy; }
    inline void setSize(Vec size) { this->vSize = size; }
    inline void setWidth(scalar width) { this->vSize.x = width; }
    inline void setHeight(scalar height) { this->vSize.y = height; }

    bool operator==(const McRectBase &rhs) const { return (this->vMin == rhs.vMin) && (this->vSize == rhs.vSize); }

   private:
    constexpr void set(scalar x, scalar y, scalar width, scalar height, bool isCentered = false) {
        this->set(Vec(x, y), Vec(width, height), isCentered);
    }

    constexpr void set(Vec pos, Vec size, bool isCentered = false) {
        if(isCentered) {
            this->vMin = pos - size / scalar(2);
        } else {
            this->vMin = pos;
        }
        this->vSize = size;
    }

    Vec vMin{0};
    Vec vSize{0};

    template <typename>
    friend class McRectBase;

#ifndef BUILD_TOOLS_ONLY
    template <typename, typename, typename>
    friend struct fmt::formatter;
#endif
};

using McRect = McRectBase<vec2>;
using McFRect = McRectBase<vec2>;
using McIRect = McRectBase<ivec2>;

#ifndef BUILD_TOOLS_ONLY  // avoid an unnecessary dependency on fmt when building tools only
namespace fmt {
template <typename Vec>
struct formatter<McRectBase<Vec>> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) const {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const McRectBase<Vec> &r, FormatContext &ctx) const {
        if constexpr(std::is_floating_point_v<typename Vec::value_type>) {
            return format_to(ctx.out(), "({:.2f},{:.2f}): {:.2f}x{:.2f}"_cf, r.vMin.x, r.vMin.y, r.vSize.x, r.vSize.y);
        } else {
            return format_to(ctx.out(), "({},{}): {}x{}"_cf, r.vMin.x, r.vMin.y, r.vSize.x, r.vSize.y);
        }
    }
};
}  // namespace fmt
#endif
