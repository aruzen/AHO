//
// Created by morimoto_hibiki on 2025/10/10.
//

#ifndef AHO_ALL_LAYOUTS_H
#define AHO_ALL_LAYOUTS_H

#include "AHO/define.hpp"

#include <optional>
#include <VSL/vulkan/pipeline_layout.hpp>
#include <VSL/vulkan/stages/resource_binding.hpp>

#include "AHO/engine.hpp"
#include "AHO/window.hpp"

#include "AHO/engine/graphical_engine.hpp"

namespace AHO_NAMESPACE::pipeline {
    enum class ResourceName {
        VP_MATRIX_UBO,
        TEXTURE,
    };

    namespace standard_resources {
        constexpr VSL_NAMESPACE::graphic_resource::BindingPoint VP_MATRIX_UBO{
            .binding = (std::uint32_t)-1,
            .binding_type = VSL_NAMESPACE::graphic_resource::Type::UNIFORM_BUFFER,
            .shader_type = VSL_NAMESPACE::ShaderFlag::VERTEX,
            .amount = 1};

        constexpr VSL_NAMESPACE::graphic_resource::BindingPoint TEXTURE{
            .binding = (std::uint32_t)-1,
            .binding_type = VSL_NAMESPACE::graphic_resource::Type::COMBINED_IMAGE_SAMPLER,
            .shader_type = VSL_NAMESPACE::ShaderFlag::FRAGMENT,
            .amount = 1};
    }; // namespace standard_resources

    typedef VSL_NAMESPACE::PipelineLayout (*GetBaseLayoutFunc)(engine::EngineAccessor* engine);

    extern GetBaseLayoutFunc GET_BASE_LAYOUT;

    constexpr auto get_binding_point(ResourceName name) {
        switch (name) {
            case ResourceName::VP_MATRIX_UBO:
                return standard_resources::VP_MATRIX_UBO;
            case ResourceName::TEXTURE:
                return standard_resources::TEXTURE;
        }
        throw std::runtime_error("error: unknown resource.");
    }

    template<ResourceName name>
    constexpr auto get_binding_point() {
        if constexpr (ResourceName::VP_MATRIX_UBO == name)
            return standard_resources::VP_MATRIX_UBO;
        if constexpr (ResourceName::TEXTURE == name)
            return standard_resources::TEXTURE;
    }

} // namespace AHO_NAMESPACE::pipeline

#endif //AHO_ALL_LAYOUTS_H
