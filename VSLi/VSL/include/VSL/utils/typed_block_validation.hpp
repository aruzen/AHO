#pragma once
#include "typed_block_layout.hpp"
#include "spir_reflector.hpp"

namespace VSL_NAMESPACE::typed {
    namespace detail {
        struct DescriptorRequirement;
        struct PushRequirement;
        void validate_cpp_block(const utils::SPIRVReflector::Block& shader, const BlockType& cpp, size_t payload_size, const std::string& context);
        void validate_descriptor_blocks(const utils::SPIRVReflector::GenerateData& shader, std::span<const DescriptorRequirement> requirements);
        void validate_push_blocks(const utils::SPIRVReflector::GenerateData& shader, std::span<const PushRequirement> requirements);
    } // namespace detail
    template<typename Layout>
    void validate_block_layout(const utils::SPIRVReflector::Block& shader) {
        using Payload = typename PayloadLayout<Layout>::Payload;
        static_assert(detail::block_layout_known(PayloadLayout<Layout>::LAYOUT), "Typed block validation requires BlockRecord/BlockLayout metadata");
        detail::validate_cpp_block(shader, PayloadLayout<Layout>::LAYOUT, sizeof(Payload), shader.name);
    }
} // namespace VSL_NAMESPACE::typed
