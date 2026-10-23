#pragma once
#include "typed_vertex_layout.hpp"
#include "spir_reflector.hpp"

namespace VSL_NAMESPACE::typed {
    namespace detail {
        void validate_vertex_shader(const utils::SPIRVReflector::GenerateData& shader, std::span<const VertexAttributeLayout> attributes);
        void validate_pipeline_vertex_shader(PipelineLayoutAccessor layout, std::span<const VertexAttributeLayout> attributes);
    } // namespace detail

    // CPU-only validation; automatic pipeline construction uses the exact loaded module and entry point.
    template<typename Input>
    void validate_vertex_shader(const utils::SPIRVReflector::GenerateData& shader) {
        detail::validate_vertex_shader(shader, Input::LAYOUT.attribute);
    }
} // namespace VSL_NAMESPACE::typed
