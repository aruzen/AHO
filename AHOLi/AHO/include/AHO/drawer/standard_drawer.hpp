//
// Created by morimoto_hibiki on 2025/10/22.
//

#ifndef AHO_ALL_STANDARDDRAWER_H
#define AHO_ALL_STANDARDDRAWER_H

#include "AHO/drawer.hpp"

#include "AHO/core/color.hpp"
#include "AHO/core/triangle.hpp"

namespace AHO_NAMESPACE {
    struct DrawPhase;
    // 座標はNDC。色は線形RGBA。現在のDrawPhaseへ即時に記録する。
    void draw(const Triangle<float, VSL_NAMESPACE::D2>& triangle, const RGBA& rgba);
    void draw(const Triangle<float, VSL_NAMESPACE::D2>& triangle, const RGBA& rgba, DrawPhase& phase);
    void draw(const Triangle<float, VSL_NAMESPACE::D2>& triangle, const RGBA& rgba, DrawPhase* phase);
} // namespace AHO_NAMESPACE

#endif //AHO_ALL_STANDARDDRAWER_H
