//
// Created by morimoto_hibiki on 2025/07/30.
//

#ifndef AHO_ALL_PIPELINE_BARRIER_HPP
#define AHO_ALL_PIPELINE_BARRIER_HPP


#include "../../define.h"
#include "../pv.h"
#include "../command.h"

#include "../descriptor.hpp"
#include "../pipeline.h"

#include "../image.hpp"

namespace VSL_NAMESPACE::command {
    struct PipelineBarrierChange : public __Command {
        PipelineBarrierChange(Image image);

        Image image;

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };
}


#endif //AHO_ALL_PIPELINE_BARRIER_HPP
