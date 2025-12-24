//
// Created by morimoto_hibiki on 2025/10/22.
//

#ifndef AHO_ALL_STANDARDDRAWER_H
#define AHO_ALL_STANDARDDRAWER_H

#include "AHO/drawer.hpp"

#include "AHO/core/color.hpp"
#include "AHO/core/triangle.hpp"

namespace AHO_NAMESPACE {
    template<>
    struct Drawer<Triangle<float, vsl::D2>, RGBA> {
        template<typename PhaseStream>
        void draw(DrawStream<PhaseStream>& ds, const Triangle<float, vsl::D2>& triangle, const RGBA& rgba) {
            // TODO ds.phase_stream <<
        }
    };
}

#endif //AHO_ALL_STANDARDDRAWER_H
