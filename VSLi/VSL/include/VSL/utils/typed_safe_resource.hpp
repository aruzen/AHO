//
// Created by morimoto_hibiki on 2025/10/24.
//

#ifndef AHO_ALL_TYPED_SAFE_RESOURCE_H
#define AHO_ALL_TYPED_SAFE_RESOURCE_H

#include "../vulkan/descriptor_set_manager.hpp"
#include "../vulkan/stages/vertex_input.hpp"
#include "../vulkan/stages/push_contents.hpp"
#include "typed_vertex_layout.hpp"

namespace VSL_NAMESPACE::typed {
    template<std::uint32_t binding, graphic_resource::Type binding_type, ShaderFlag shader_type, std::uint32_t amount = 1>
    struct ResourceBindingPoint {
        static constexpr graphic_resource::BindingPoint VALUE = {binding, binding_type, shader_type, amount};
    };

    template<typename... Points>
    struct ResourceBindingPoints {
        static constexpr size_t Size() { return sizeof...(Points); }

        template<size_t index>
        static constexpr auto Get() {
            return std::get<index>(std::tuple{Points::VALUE...});
        }

        graphic_resource::BindingLayout make(LogicalDeviceAccessor device) {
            return {device, {Points::VALUE...}};
        }
    };

    template<VSL_NAMESPACE::ShaderFlag target, std::uint32_t size = (std::uint32_t)-1, std::uint32_t offset = (std::uint32_t)-1>
    struct PushContentDefinitionTag {
        static constexpr pipeline_layout::PushContentDefinition VALUE = {target, offset, size};
    };

    template<typename... Definitions>
    struct PushContentDefinitions {
        static constexpr size_t Size() { return sizeof...(Definitions); }

        template<size_t index>
        static constexpr auto Get() {
            return std::get<index>(std::tuple{Definitions::VALUE...});
        }
    };

} // namespace VSL_NAMESPACE::typed

#endif //AHO_ALL_TYPED_SAFE_RESOURCE_H
