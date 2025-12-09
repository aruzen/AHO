//
// Created by morimoto_hibiki on 2025/10/24.
//

#ifndef AHO_ALL_TYPED_SAFE_RESOURCE_H
#define AHO_ALL_TYPED_SAFE_RESOURCE_H

#include "../vulkan/descriptor_set_manager.hpp"
#include "../vulkan/stages/vertex_input.hpp"
#include "../vulkan/stages/push_contents.hpp"

namespace VSL_NAMESPACE::typed {
    template<std::uint32_t Binding, graphic_resource::Type BindingType, ShaderFlag ShaderType, std::uint32_t Amount = 1>
    struct ResourceBindingPoint {
        static constexpr graphic_resource::BindingPoint value = {Binding, BindingType, ShaderType, Amount};
    };

    template<typename... Points>
    struct ResourceBindingPoints {
        static constexpr size_t size() { return sizeof...(Points); }

        template<size_t Index>
        static constexpr auto get() {
            return std::get<Index>(std::tuple{Points::value...});
        }

        graphic_resource::BindingLayout make(LogicalDeviceAccessor device) {
            return {device, {Points::value...}};
        }
    };

    template<data_format::___Format FormatType, std::uint32_t Location = (std::uint32_t) -1, std::uint32_t Offset = (std::uint32_t) -1>
    struct VertexInputDefinition {
        static constexpr pipeline_layout::VertexInputLayoutDefinition value = {FormatType, Location, Offset};
    };

    template<std::uint32_t Binding, std::uint32_t Size, pipeline_layout::VertexInputShapeDefinition::UpdateTiming Timing, typename...Definitions>
    struct VertexShapeDefinition {
        static constexpr size_t size() { return sizeof...(Definitions); }

        template<size_t Index>
        static constexpr auto get() {
            return std::get<Index>(std::tuple{Definitions::value...});
        }
    };

    template<typename...Definitions>
    struct TypedVertexInput {
        void injection(VSL_NAMESPACE::PipelineLayoutAccessor pl);
    };

    template<VSL_NAMESPACE::ShaderFlag Target, std::uint32_t Size = (std::uint32_t) -1, std::uint32_t Offset = (std::uint32_t) -1>
    struct PushContentDefinitionTag {
        static constexpr pipeline_layout::PushContentDefinition value = {Target, Offset, Size};
    };

    template<typename...Definitions>
    struct PushContentDefinitions {
        static constexpr size_t size() { return sizeof...(Definitions); }

        template<size_t Index>
        static constexpr auto get() {
            return std::get<Index>(std::tuple{Definitions::value...});
        }
    };

    // ===========================================================

    template<typename... Definitions>
    void TypedVertexInput<Definitions...>::injection(vsl::PipelineLayoutAccessor pl) {
        
    }
}

#endif //AHO_ALL_TYPED_SAFE_RESOURCE_H
