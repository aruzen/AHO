//
// Created by morimoto_hibiki on 2025/10/10.
//

#ifndef AHO_ALL_LAYOUTS_H
#define AHO_ALL_LAYOUTS_H

#include "AHO/define.hpp"

#include <optional>

#include "AHO/engine.hpp"
#include "AHO/window.hpp"

#include "AHO/engine/graphical_engine.hpp"

namespace AHO_NAMESPACE::pipeline {
    enum class ResourceName {
        VPMatrixUBO,
        Texture,
    };

    namespace standard_resources {
        constexpr VSL_NAMESPACE::graphic_resource::BindingPoint VPMatrixUBO{
            .binding = (std::uint32_t)-1,
            .bindingType = VSL_NAMESPACE::graphic_resource::Type::UniformBuffer,
            .shaderType = VSL_NAMESPACE::ShaderFlag::Vertex,
            .amount = 1};

        constexpr VSL_NAMESPACE::graphic_resource::BindingPoint Texture{
            .binding = (std::uint32_t)-1,
            .bindingType = VSL_NAMESPACE::graphic_resource::Type::CombinedImageSampler,
            .shaderType = VSL_NAMESPACE::ShaderFlag::Fragment,
            .amount = 1};
    }; // namespace standard_resources

    constexpr auto getBindingPoint(ResourceName name) {
        switch (name) {
            case ResourceName::VPMatrixUBO:
                return standard_resources::VPMatrixUBO;
            case ResourceName::Texture:
                return standard_resources::Texture;
        }
        throw std::runtime_error("error: unknown resource.");
    }

    template<ResourceName Name>
    constexpr auto getBindingPoint() {
        if constexpr (ResourceName::VPMatrixUBO == Name)
            return standard_resources::VPMatrixUBO;
        if constexpr (ResourceName::Texture == Name)
            return standard_resources::Texture;
    }

    typedef VSL_NAMESPACE::PipelineLayout (*GET_BASE_LAYOUT_func)(engine::EngineAccessor* engine);

    extern GET_BASE_LAYOUT_func GET_BASE_LAYOUT;
} // namespace AHO_NAMESPACE::pipeline

#endif //AHO_ALL_LAYOUTS_H
