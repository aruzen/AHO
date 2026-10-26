//
// Created by morimoto_hibiki on 2025/07/30.
//

#ifndef AHO_ALL_BIND_GRAPHIC_RESOURCE_HPP
#define AHO_ALL_BIND_GRAPHIC_RESOURCE_HPP

#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"
#include "../resource_use.hpp"

#include "../descriptor.hpp"
#include "../pipeline.hpp"

namespace VSL_NAMESPACE::command {
    struct BindGraphicResource : public Command, public PipelineRequire {
        BindGraphicResource(GraphicResource resource, graphic_resource::BindingDestination dst, std::optional<PipelineAccessor> pipeline = std::nullopt, std::uint32_t first_binding = 0, std::shared_ptr<ResourceUseGroup> use = {});
        BindGraphicResource(const std::vector<GraphicResource>& resources, graphic_resource::BindingDestination dst, std::optional<PipelineAccessor> pipeline = std::nullopt, std::uint32_t first_binding = 0, std::shared_ptr<ResourceUseGroup> use = {});

        std::vector<GraphicResource> resources;
        graphic_resource::BindingDestination destination;
        std::optional<PipelineAccessor> pipeline;
        std::uint32_t first_binding;

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;
        void track(const CommandBuffer& buffer) const;

        // ストリームの指定を優先し、空なら構築時のpipelineを使う。
        void set_pipeline(std::optional<PipelineAccessor> pipeline) override;

    private:
        std::optional<PipelineAccessor> _fallback_pipeline = pipeline;
        std::shared_ptr<ResourceUseGroup> _use;
    };
} // namespace VSL_NAMESPACE::command

#endif //AHO_ALL_BIND_GRAPHIC_RESOURCE_HPP
