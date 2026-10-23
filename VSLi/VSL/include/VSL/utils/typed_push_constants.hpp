#pragma once
#include "../vulkan/stages/push_contents.hpp"
#include "../vulkan/pipeline.hpp"
#include "../vulkan/command.hpp"
#include <array>
#include <concepts>
#include <cstring>
#include <optional>
#include <span>
#include <tuple>
#include <type_traits>

namespace VSL_NAMESPACE::typed {
    namespace detail {
        struct PushRequirement {
            pipeline_layout::PushContentDefinition range;
            size_t data_offset;
        };
        void validate_push_values(std::span<const PushRequirement> requirements, std::span<const std::byte> values);
        void inject_push_schema(PipelineLayoutEditAccessor layout, std::span<const PushRequirement> requirements);
        void validate_push_schema(PipelineLayoutAccessor layout, std::span<const PushRequirement> requirements);
        void invoke_push_values(PipelineAccessor pipeline, CommandBuffer buffer, std::span<const PushRequirement> requirements, std::span<const std::byte> values);
        template<typename Schema, typename... Values, size_t... indices>
        consteval bool push_values_match(std::index_sequence<indices...>);
    } // namespace detail
    template<typename Schema>
    class PushValues;

    template<typename Payload_, ShaderFlag stages, std::uint32_t offset = 0>
    struct PushRange {
        using Payload = Payload_;
        static_assert(std::is_standard_layout_v<Payload> && std::is_trivially_copyable_v<Payload>, "Typed push payload must be a trivial standard-layout record");
        static_assert(sizeof(Payload) % 4 == 0 && offset % 4 == 0 && std::uint64_t(offset) + sizeof(Payload) <= UINT32_MAX,
                      "Invalid typed push offset/size: 4-byte alignment and uint32 extent required");
        static_assert(stages != ShaderFlag::ERROR_, "Typed push stages must not be empty");
        static constexpr pipeline_layout::PushContentDefinition RANGE{stages, offset, sizeof(Payload)};
    };
    template<typename... Ranges_>
    struct PushSchema {
        using Ranges = std::tuple<Ranges_...>;
        using Values = PushValues<PushSchema>;
        static constexpr size_t COUNT = sizeof...(Ranges_);
        static constexpr bool EMPTY = COUNT == 0;
        static constexpr size_t DATA_SIZE = (sizeof(typename Ranges_::Payload) + ... + 0);
        template<size_t index>
        using Range = std::tuple_element_t<index, Ranges>;
        static consteval auto MakeRequirements();
        static constexpr auto REQUIREMENTS = MakeRequirements();
        static consteval bool IsValid();
        static_assert(IsValid(), "Overlapping typed push ranges for the same shader stage");
        void injection(PipelineLayoutEditAccessor layout) const { detail::inject_push_schema(layout, REQUIREMENTS); }
    };
    template<typename Schema>
    class PushValues {
        alignas(4) std::array<std::byte, Schema::DATA_SIZE> _bytes{};

    public:
        template<typename... Values>
        requires(detail::push_values_match<Schema, Values...>(std::index_sequence_for<Values...>{}))
        explicit PushValues(const Values&... values);
        std::span<const std::byte> bytes() const { return _bytes; }
    };
    namespace detail {
        template<typename Schema, typename... Values, size_t... indices>
        consteval bool push_values_match(std::index_sequence<indices...>) {
            if constexpr (sizeof...(Values) != Schema::COUNT)
                return false;
            else
                return (std::same_as<std::remove_cvref_t<Values>, typename Schema::template Range<indices>::Payload> && ...);
        }
    } // namespace detail

    template<typename... RangeTypes>
    consteval auto PushSchema<RangeTypes...>::MakeRequirements() {
        std::array<detail::PushRequirement, sizeof...(RangeTypes)> result{};
        size_t offset = 0, index = 0;
        auto add = [&]<typename Range>() { result[index++] = {Range::RANGE, offset}; offset += sizeof(typename Range::Payload); };
        (add.template operator()<RangeTypes>(), ...);
        return result;
    }
    template<typename... RangeTypes>
    consteval bool PushSchema<RangeTypes...>::IsValid() {
        for (size_t i = 0; i < REQUIREMENTS.size(); ++i) {
            const auto& a = REQUIREMENTS[i].range;
            for (size_t j = 0; j < i; ++j) {
                const auto& b = REQUIREMENTS[j].range;
                if ((a.target_shader & b.target_shader) != ShaderFlag::ERROR_ &&
                    a.offset < std::uint64_t(b.offset) + b.size && b.offset < std::uint64_t(a.offset) + a.size)
                    return false;
            }
        }
        return true;
    }
    template<typename Schema>
    template<typename... Values>
    requires(detail::push_values_match<Schema, Values...>(std::index_sequence_for<Values...>{}))
    PushValues<Schema>::PushValues(const Values&... values) {
        size_t offset = 0;
        auto copy = [&](const auto& value) { std::memcpy(_bytes.data() + offset, &value, sizeof(value)); offset += sizeof(value); };
        (copy(values), ...);
        detail::validate_push_values(Schema::REQUIREMENTS, _bytes);
    }
} // namespace VSL_NAMESPACE::typed
