#pragma once
#include "../utils/typed_resource_allocation.hpp"
#include <span>

namespace VSL_NAMESPACE::typed {
    struct DescriptorLayoutPlan {
        std::uint32_t set;
        std::span<const graphic_resource::BindingPoint> bindings;
    };
    struct DescriptorSetPlan {
        std::uint32_t set;
        size_t layout;
    };
    struct DescriptorTypeCapacity {
        graphic_resource::Type type;
        std::uint32_t count;
    };
    struct DescriptorPoolPlan {
        std::uint32_t max_sets;
        std::span<const DescriptorTypeCapacity> descriptors;
    };

    // 宣言数で保持する論理計画。native生成用コンテナとResourceId/slot割当は含めない。
    template<typename Schema_>
    class TypedDescriptorPlan {
        using Resources = typename Schema_::Resources;
        static consteval auto MakeLayouts();
        static constexpr auto LAYOUTS = MakeLayouts();
        std::array<std::array<DescriptorSetPlan, Resources::COUNT>, 2> _sets{};
        std::array<std::array<DescriptorTypeCapacity, Resources::REQUIREMENTS.size()>, 2> _capacities{};
        std::array<size_t, 2> _set_counts{}, _type_counts{};
        size_t _frames, _total_sets = 0, _total_pools = 0, _total_descriptors = 0;
        static constexpr size_t ScopeIndex(ResourceScope scope);
        constexpr void add_capacity(size_t group, graphic_resource::Type type, std::uint32_t count);

    public:
        using Schema = Schema_;
        constexpr explicit TypedDescriptorPlan(size_t frames);
        constexpr explicit TypedDescriptorPlan(const ResourceAllocationPlan<Schema>& plan) : TypedDescriptorPlan(plan.frame_count()) {}
        static constexpr std::span<const DescriptorLayoutPlan> Layouts() { return LAYOUTS; }
        constexpr size_t frame_count() const { return _frames; }
        constexpr size_t total_sets() const { return _total_sets; }
        constexpr size_t total_pools() const { return _total_pools; }
        constexpr size_t total_descriptors() const { return _total_descriptors; }
        constexpr std::span<const DescriptorSetPlan> sets(ResourceScope scope) const&;
        constexpr std::span<const DescriptorSetPlan> sets(ResourceScope scope) const&& = delete;
        constexpr DescriptorPoolPlan pool(ResourceScope scope) const&;
        constexpr DescriptorPoolPlan pool(ResourceScope scope) const&& = delete;
    };

    template<typename Schema>
    consteval auto TypedDescriptorPlan<Schema>::MakeLayouts() {
        return []<size_t... indices>(std::index_sequence<indices...>) {
            return std::array<DescriptorLayoutPlan, Resources::COUNT>{{{Resources::template Set<indices>::SET, Resources::template Set<indices>::POINTS}...}};
        }(std::make_index_sequence<Resources::COUNT>{});
    }
    template<typename Schema>
    constexpr size_t TypedDescriptorPlan<Schema>::ScopeIndex(ResourceScope scope) {
        if (scope == ResourceScope::SHARED)
            return 0;
        if (scope == ResourceScope::PER_FRAME)
            return 1;
        throw std::invalid_argument("Unknown descriptor lifetime scope");
    }
    template<typename Schema>
    constexpr void TypedDescriptorPlan<Schema>::add_capacity(size_t group, graphic_resource::Type type, std::uint32_t count) {
        size_t index = 0;
        while (index < _type_counts[group] && _capacities[group][index].type != type)
            ++index;
        if (index == _type_counts[group])
            _capacities[group][_type_counts[group]++] = {type, 0};
        auto& capacity = _capacities[group][index].count;
        if (count > UINT32_MAX - capacity)
            throw std::out_of_range("Descriptor pool type capacity exceeds uint32_t");
        capacity += count;
    }
    template<typename Schema>
    constexpr TypedDescriptorPlan<Schema>::TypedDescriptorPlan(size_t frames) : _frames(frames) {
        if (!frames)
            throw std::invalid_argument("Typed descriptor frame count must be positive");
        for (size_t layout = 0; layout < LAYOUTS.size(); ++layout) {
            const auto& declaration = LAYOUTS[layout];
            const auto group = ScopeIndex(Schema::SetIsPerFrame(declaration.set) ? ResourceScope::PER_FRAME : ResourceScope::SHARED);
            if (_set_counts[group] == UINT32_MAX)
                throw std::out_of_range("Descriptor pool maxSets exceeds uint32_t");
            _sets[group][_set_counts[group]++] = {declaration.set, layout};
            for (const auto& binding : declaration.bindings)
                add_capacity(group, binding.binding_type, binding.amount);
        }
        for (size_t group = 0; group < 2; ++group) {
            const auto instances = group ? frames : 1;
            _total_sets = detail::allocation_add(_total_sets, detail::allocation_multiply(_set_counts[group], instances));
            if (_set_counts[group])
                _total_pools = detail::allocation_add(_total_pools, instances);
            for (size_t type = 0; type < _type_counts[group]; ++type)
                _total_descriptors = detail::allocation_add(_total_descriptors, detail::allocation_multiply(_capacities[group][type].count, instances));
        }
    }
    template<typename Schema>
    constexpr std::span<const DescriptorSetPlan> TypedDescriptorPlan<Schema>::sets(ResourceScope scope) const& {
        const auto group = ScopeIndex(scope);
        return {_sets[group].data(), _set_counts[group]};
    }
    template<typename Schema>
    constexpr DescriptorPoolPlan TypedDescriptorPlan<Schema>::pool(ResourceScope scope) const& {
        const auto group = ScopeIndex(scope);
        return {static_cast<std::uint32_t>(_set_counts[group]), {_capacities[group].data(), _type_counts[group]}};
    }
} // namespace VSL_NAMESPACE::typed
