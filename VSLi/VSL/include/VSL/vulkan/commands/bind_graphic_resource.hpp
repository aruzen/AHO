//
// Created by morimoto_hibiki on 2025/07/30.
//

#ifndef AHO_ALL_BIND_GRAPHIC_RESOURCE_HPP
#define AHO_ALL_BIND_GRAPHIC_RESOURCE_HPP


#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include "../descriptor.hpp"
#include "../pipeline.hpp"

namespace VSL_NAMESPACE::command {
    struct BindGraphicResource : public __Command, public __PipelineRequire {
        BindGraphicResource(GraphicResource resource, graphic_resource::BindingDestination dst, std::optional<PipelineAccessor> pipeline = std::nullopt);
        BindGraphicResource(const std::vector<GraphicResource>& resources, graphic_resource::BindingDestination dst, std::optional<PipelineAccessor> pipeline = std::nullopt);

        std::vector<GraphicResource> resources;
        graphic_resource::BindingDestination destination;
        std::optional<PipelineAccessor> pipeline;

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);

        virtual void setPipeline(std::optional<PipelineAccessor> pipeline);
    };
}


#endif //AHO_ALL_BIND_GRAPHIC_RESOURCE_HPP
