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
        MPVMatrixUBO,
    };

    namespace  standard_resources {
        constexpr vsl::graphic_resource::BindingPoint MPVMatrixUBO{
                    .binding = (std::uint32_t) -1,
                    .bindingType = vsl::graphic_resource::Type::UniformBuffer,
                    .shaderType = vsl::ShaderFlag::Vertex,
                    .amount = 1
            };
    };

    constexpr auto getBindingPoint(ResourceName name) {
        if (ResourceName::MPVMatrixUBO == name) {
            return standard_resources::MPVMatrixUBO;
        }
        throw std::runtime_error("error: unknown resource.");
    }

    template<ResourceName Name>
    constexpr auto getBindingPoint() {
        if constexpr (ResourceName::MPVMatrixUBO == Name) {
            return standard_resources::MPVMatrixUBO;
        } else {
            static_assert(false, "error: unknown resource.");
        }
    }

    typedef VSL_NAMESPACE::PipelineLayout (*GET_BASE_LAYOUT_func)(engine::EngineAccessor *engine);

    extern GET_BASE_LAYOUT_func GET_BASE_LAYOUT;
}

#endif //AHO_ALL_LAYOUTS_H
