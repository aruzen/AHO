// Created by morimoto_hibiki on 2026/01/03.
//

#ifndef AHO_ALL_ID_PICKING_HPP
#define AHO_ALL_ID_PICKING_HPP

#include "../../define.hpp"
#include "../pv.hpp"
#include "../pipeline_layout.hpp"

namespace VSL_NAMESPACE::pipeline_layout {
    struct IDPicking {
        void injection(VSL_NAMESPACE::PipelineLayoutAccessor pl);
    };
}


#endif //AHO_ALL_ID_PICKING_HPP
