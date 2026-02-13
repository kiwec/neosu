// Copyright (c) 2012, PG, All rights reserved.
#include "Rect.h"

McRect McRect::intersect(const McRect &rect) const {
    McRect intersection;

    vec2 thisMax = this->vMin + this->vSize;
    vec2 rectMax = rect.vMin + rect.vSize;

    intersection.vMin = vec::max(this->vMin, rect.vMin);
    vec2 intersectMax = vec::min(thisMax, rectMax);

    if(vec::any(vec::greaterThan(intersection.vMin, intersectMax))) {
        intersection.vMin = {0.f, 0.f};
        intersection.vSize = {0.f, 0.f};
    } else {
        intersection.vSize = intersectMax - intersection.vMin;
    }

    return intersection;
}

McRect McRect::Union(const McRect &other) const {
    const vec2 vMin = vec::min(this->vMin, other.vMin);
    return {vMin, {vec::max(this->getMax(), other.getMax()) - vMin}};
}
