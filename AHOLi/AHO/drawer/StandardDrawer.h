//
// Created by morimoto_hibiki on 2025/10/22.
//

#ifndef AHO_ALL_STANDARDDRAWER_H
#define AHO_ALL_STANDARDDRAWER_H

#include "../Drawer.h"

#include "../core/color.h"
#include "../core/Triangle.h"

namespace AHO_NAMESPACE {
    template<>
    struct Drawer<Triangle<float, vsl::D2>, RGBA> {
        template<typename PhaseStream>
        void draw(DrawStream<PhaseStream>& ds, const Triangle<float, vsl::D2>& triangle, const RGBA& rgba) {
            ds.phase_stream <<
        }
    };
}

#endif //AHO_ALL_STANDARDDRAWER_H
