//
// Created by morimoto_hibiki on 2025/10/22.
//

#ifndef AHO_ALL_STANDARDDRAWER_H
#define AHO_ALL_STANDARDDRAWER_H

#include "AHO/drawer.hpp"

#include "AHO/core/color.hpp"
#include "AHO/core/triangle.hpp"

namespace AHO_NAMESPACE {
    void draw(const Triangle<float, vsl::D2> &triangle, const RGBA &rgba);
}

#endif //AHO_ALL_STANDARDDRAWER_H
