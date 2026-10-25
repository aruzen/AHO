#pragma once
#include "typed_block_layout.hpp"
#include "../vulkan/stages/resource_binding.hpp"
#include <algorithm>
#include <array>
#include <concepts>
#include <span>
#include <stdexcept>
#include <tuple>
#include <type_traits>

namespace VSL_NAMESPACE::typed {
    namespace detail {
        struct DescriptorRequirement {
            std::uint32_t set;
            graphic_resource::BindingPoint point;
            const BlockType* memory_layout = nullptr;
            size_t payload_size = 0;
        };
        void inject_resource_schema(PipelineLayoutEditAccessor layout, std::span<const DescriptorRequirement> requirements, std::uint32_t set_count);
        void validate_resource_schema(PipelineLayoutAccessor layout, std::span<const DescriptorRequirement> requirements);
        template<std::uint32_t binding, typename... Bindings>
        struct FindDescriptor;
        template<std::uint32_t binding>
        struct FindDescriptor<binding> {
            using Type = void;
        };
        template<std::uint32_t binding, typename First, typename... Rest>
        struct FindDescriptor<binding, First, Rest...> {
            using Type = std::conditional_t<First::POINT.binding == binding, First, typename FindDescriptor<binding, Rest...>::Type>;
        };
    } // namespace detail

    // Schema types describe descriptors; allocation and updates live in typed_resources.hpp.
    template<std::uint32_t binding, graphic_resource::Type kind, ShaderFlag stages, typename Payload_ = void, std::uint32_t count = 1>
    struct DescriptorBinding {
        using MemoryLayout = PayloadLayout<Payload_>;
        using Payload = typename MemoryLayout::Payload;
        static constexpr const BlockType* MEMORY_LAYOUT = std::is_void_v<Payload> ? nullptr : &MemoryLayout::LAYOUT;
        static constexpr size_t PAYLOAD_SIZE = [] {
            if constexpr (std::is_void_v<Payload>)
                return size_t{0};
            else
                return sizeof(Payload);
        }();
        static_assert(std::is_void_v<Payload> || detail::block_layout_known(MemoryLayout::LAYOUT), "Typed descriptor payload requires BlockRecord/BlockLayout metadata");
        static_assert(MemoryLayout::LAYOUT.size <= PAYLOAD_SIZE, "Typed descriptor layout exceeds payload sizeof");
        static constexpr graphic_resource::BindingPoint POINT{binding, kind, stages, count};
        static_assert(std::is_void_v<Payload> || (std::is_trivially_copyable_v<Payload> && std::is_standard_layout_v<Payload>), "Typed descriptor payload must be a trivial standard-layout record");
        static_assert(binding != UINT32_MAX && count != 0 && stages != ShaderFlag::ERROR_, "Invalid typed descriptor binding/count/stages");
        static_assert(kind == graphic_resource::Type::UNIFORM_BUFFER || kind == graphic_resource::Type::STORAGE_BUFFER ||
                          kind == graphic_resource::Type::COMBINED_IMAGE_SAMPLER || kind == graphic_resource::Type::SAMPLED_IMAGE ||
                          kind == graphic_resource::Type::STORAGE_IMAGE || kind == graphic_resource::Type::SAMPLER,
                      "Unsupported typed descriptor kind");
        static_assert((kind == graphic_resource::Type::UNIFORM_BUFFER || kind == graphic_resource::Type::STORAGE_BUFFER) != std::is_void_v<Payload>,
                      "Typed buffer descriptors require a payload; image descriptors use void");
    };
    template<std::uint32_t binding, typename Payload, ShaderFlag stages, std::uint32_t count = 1>
    using UniformBinding = DescriptorBinding<binding, graphic_resource::Type::UNIFORM_BUFFER, stages, Payload, count>;
    template<std::uint32_t binding, typename Payload, ShaderFlag stages, std::uint32_t count = 1>
    using StorageBinding = DescriptorBinding<binding, graphic_resource::Type::STORAGE_BUFFER, stages, Payload, count>;
    template<std::uint32_t binding, ShaderFlag stages, std::uint32_t count = 1>
    using TextureBinding = DescriptorBinding<binding, graphic_resource::Type::COMBINED_IMAGE_SAMPLER, stages, void, count>;

    template<typename... Bindings_>
    struct DescriptorLayout {
        using Layout = DescriptorLayout;
        using Bindings = std::tuple<Bindings_...>;
        static constexpr std::array<graphic_resource::BindingPoint, sizeof...(Bindings_)> POINTS{Bindings_::POINT...};
        static constexpr size_t BINDING_COUNT = sizeof...(Bindings_);
        static constexpr std::array<const BlockType*, BINDING_COUNT> MEMORY_TYPES{Bindings_::MEMORY_LAYOUT...};
        static constexpr std::array<size_t, BINDING_COUNT> PAYLOAD_SIZES{Bindings_::PAYLOAD_SIZE...};
        static consteval bool IsValid();
        static_assert(IsValid(), "Duplicate typed descriptor binding");
        template<std::uint32_t binding>
        using Binding = typename detail::FindDescriptor<binding, Bindings_...>::Type;
        static constexpr size_t BindingIndex(std::uint32_t binding);
    };

    template<std::uint32_t set, typename Layout_>
    struct DescriptorSetBinding : Layout_ {
        using Layout = Layout_;
        static constexpr std::uint32_t SET = set;
        static_assert(set < UINT32_MAX, "Invalid typed descriptor set index");
    };
    template<std::uint32_t set, typename... Bindings>
    using DescriptorSet = DescriptorSetBinding<set, DescriptorLayout<Bindings...>>;

    template<typename... Sets_>
    struct ResourceSchema {
        using Sets = std::tuple<Sets_...>;
        static constexpr size_t COUNT = sizeof...(Sets_);
        static constexpr std::array<std::uint32_t, COUNT> SET_NUMBERS{Sets_::SET...};
        static constexpr std::uint32_t SET_COUNT = std::max({0u, (Sets_::SET + 1)...});
        static constexpr bool EMPTY = COUNT == 0;
        static consteval auto MakeRequirements();
        static constexpr auto REQUIREMENTS = MakeRequirements();
        static consteval bool IsValid();
        static_assert(IsValid(), "Duplicate typed descriptor set");
        template<size_t index>
        using Set = std::tuple_element_t<index, Sets>;
        void injection(PipelineLayoutEditAccessor layout) const { detail::inject_resource_schema(layout, REQUIREMENTS, SET_COUNT); }
    };

    template<typename... BindingTypes>
    consteval bool DescriptorLayout<BindingTypes...>::IsValid() {
        for (size_t i = 0; i < POINTS.size(); ++i)
            for (size_t j = 0; j < i; ++j)
                if (POINTS[i].binding == POINTS[j].binding)
                    return false;
        return true;
    }
    template<typename... BindingTypes>
    constexpr size_t DescriptorLayout<BindingTypes...>::BindingIndex(std::uint32_t binding) {
        for (size_t i = 0; i < POINTS.size(); ++i)
            if (POINTS[i].binding == binding)
                return i;
        throw std::invalid_argument("Typed descriptor binding not found");
    }
    template<typename... SetTypes>
    consteval auto ResourceSchema<SetTypes...>::MakeRequirements() {
        std::array<detail::DescriptorRequirement, (SetTypes::BINDING_COUNT + ... + 0)> result{};
        size_t index = 0;
        auto add = [&]<typename Set>() {
            for (size_t i = 0; i < Set::POINTS.size(); ++i)
                result[index++] = {Set::SET, Set::POINTS[i], Set::MEMORY_TYPES[i], Set::PAYLOAD_SIZES[i]};
        };
        (add.template operator()<SetTypes>(), ...);
        return result;
    }
    template<typename... SetTypes>
    consteval bool ResourceSchema<SetTypes...>::IsValid() {
        constexpr std::array<std::uint32_t, sizeof...(SetTypes)> sets{SetTypes::SET...};
        for (size_t i = 0; i < sets.size(); ++i)
            for (size_t j = 0; j < i; ++j)
                if (sets[i] == sets[j])
                    return false;
        return true;
    }
} // namespace VSL_NAMESPACE::typed
