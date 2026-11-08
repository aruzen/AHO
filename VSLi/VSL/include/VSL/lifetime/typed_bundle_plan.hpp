#pragma once
#include "typed_buffer_arena.hpp"
#include "resource_plan.hpp"
#include "typed_descriptor_plan.hpp"
#include "typed_descriptor_layouts.hpp"
#include "../utils/typed_resource_allocation.hpp"
#include <algorithm>
#include <variant>
#include <optional>

namespace VSL_NAMESPACE::typed {
    struct HeapStorage {};
    template<typename Bindings_ = ResourceBindingMap<>>
    struct ArenaStorage {
        using Bindings = Bindings_;
    };

    using BundleResourceInit = std::variant<BufferCreateInfo, ImageCreateInfo, SamplerCreateInfo>;
    struct BundleResourcePlacement {
        BindingKey binding;
        ResourceId resource;
        ResourceScope scope;
        bool external;
        size_t slot;
        ResourceComponent component;
        storage::ResourceKind kind;
        size_t offset;
    };
    using BundleBufferPlacement = BundleResourcePlacement;
    namespace detail {
        struct BundleBindingKey {
            BindingKey binding;
            ResourceComponent component;
            constexpr bool operator==(const BundleBindingKey&) const = default;
        };
        constexpr bool bundle_info_equal(const BundleResourceInit& a, const BundleResourceInit& b);
    } // namespace detail

    // binding/componentからIDへの接続と、寿命群ごとの物理配置を分ける。
    template<typename Schema, typename Bindings = ResourceBindingMap<>>
    class TypedBundlePlan {
        using Resources = typename Schema::Resources;
        using LayoutShape = detail::DescriptorLayoutShape<Resources>;
        static consteval size_t BindingCount();
        template<size_t first, size_t second>
        static consteval bool SamePayload();
        static constexpr ResourceId Identity(size_t index);
        static consteval auto MakeDescriptorIds();
        static constexpr auto KEYS = [] {
            std::array<detail::BundleBindingKey, BindingCount()> keys{};
            size_t index = 0;
            for (const auto& entry : Schema::ENTRIES)
                for (size_t element = 0; element < entry.descriptors; ++element) {
                    if (entry.allocation == AllocationKind::TEXTURE) {
                        keys[index++] = {{entry.set, entry.binding, element}, ResourceComponent::IMAGE};
                        keys[index++] = {{entry.set, entry.binding, element}, ResourceComponent::SAMPLER};
                    } else
                        keys[index++] = {{entry.set, entry.binding, element}, ResourceComponent::PRIMARY};
                }
            return keys;
        }();
        static constexpr auto DESCRIPTOR_IDS = MakeDescriptorIds();
        std::array<BundleResourcePlacement, BindingCount()> _placements{};
        std::array<std::array<ResourceRequest, BindingCount()>, 2> _requests{};
        std::array<std::array<BundleResourceInit, BindingCount()>, 2> _infos{};
        // 旧Buffer専用の計画参照を維持する移行用の一覧。
        std::array<std::array<BufferCreateInfo, BindingCount()>, 2> _buffers{};
        std::array<size_t, 2> _counts{}, _buffer_counts{};
        TypedDescriptorPlan<Schema> _descriptors;
        size_t _frames;
        static constexpr size_t ScopeIndex(ResourceScope scope);

    public:
        static constexpr size_t CAPACITY = BindingCount();
        static constexpr bool NEEDS_EMPTY_LAYOUT = LayoutShape::NEEDS_EMPTY;
        static constexpr size_t ARENA_CAPACITY = detail::allocation_add(CAPACITY, detail::allocation_add(detail::allocation_multiply(Resources::COUNT, 2), 1 + NEEDS_EMPTY_LAYOUT));
        constexpr explicit TypedBundlePlan(const ResourceAllocationPlan<Schema>& plan);
        constexpr const TypedDescriptorPlan<Schema>& descriptors() const& { return _descriptors; }
        constexpr const TypedDescriptorPlan<Schema>& descriptors() const&& = delete;
        constexpr const auto& placements() const& { return _placements; }
        constexpr const auto& placements() const&& = delete;
        constexpr std::span<const BundleResourceInit> resources(ResourceScope scope) const& {
            const auto i = ScopeIndex(scope);
            return {_infos[i].data(), _counts[i]};
        }
        constexpr std::span<const BundleResourceInit> resources(ResourceScope scope) const&& = delete;
        constexpr ResourcePlan<CAPACITY> resource_plan(ResourceScope scope, size_t instance = 0) const;
        constexpr ResourcePlan<ARENA_CAPACITY> arena_plan(ResourceScope scope, size_t instance = 0) const;
        constexpr ResourceId descriptor_layout(std::uint32_t set) const;
        constexpr ResourceId descriptor_set(std::uint32_t set) const;
        constexpr std::optional<ResourceId> empty_descriptor_layout() const;
        constexpr std::optional<ResourceId> descriptor_pool(ResourceScope scope) const;
        constexpr std::span<const BufferCreateInfo> buffers(ResourceScope scope) const& {
            const auto i = ScopeIndex(scope);
            return {_buffers[i].data(), _buffer_counts[i]};
        }
        constexpr std::span<const BufferCreateInfo> buffers(ResourceScope scope) const&& = delete;
        constexpr size_t bytes(ResourceScope scope) const { return resource_plan(scope).bytes(); }
        constexpr size_t alignment(ResourceScope scope) const { return resource_plan(scope).alignment(); }
        constexpr size_t total_bytes() const { return detail::allocation_add(bytes(ResourceScope::SHARED), detail::allocation_multiply(bytes(ResourceScope::PER_FRAME), _frames)); }
        constexpr size_t arena_bytes(ResourceScope scope) const { return arena_plan(scope).bytes(); }
        constexpr size_t total_arena_bytes() const { return detail::allocation_add(arena_bytes(ResourceScope::SHARED), detail::allocation_multiply(arena_bytes(ResourceScope::PER_FRAME), _frames)); }
        constexpr BundleResourcePlacement placement(BindingKey key, ResourceComponent component = ResourceComponent::PRIMARY) const;
    };

    namespace detail {
        constexpr bool bundle_info_equal(const BundleResourceInit& a, const BundleResourceInit& b) {
            if (a.index() != b.index())
                return false;
            if (const auto* x = std::get_if<BufferCreateInfo>(&a)) {
                const auto& y = std::get<BufferCreateInfo>(b);
                return x->bytes == y.bytes && x->usage == y.usage && x->properties == y.properties && x->sharing == y.sharing;
            }
            if (const auto* x = std::get_if<ImageCreateInfo>(&a)) {
                const auto& y = std::get<ImageCreateInfo>(b);
                return x->usage == y.usage && x->width == y.width && x->height == y.height && x->format == y.format && x->layers == y.layers && x->samples == y.samples && x->properties == y.properties && x->initial_layout == y.initial_layout;
            }
            const auto& x = std::get<SamplerCreateInfo>(a);
            const auto& y = std::get<SamplerCreateInfo>(b);
            return x.sampling == y.sampling && x.edge == y.edge;
        }
    } // namespace detail
    template<typename Schema, typename Bindings>
    constexpr size_t TypedBundlePlan<Schema, Bindings>::ScopeIndex(ResourceScope scope) {
        if (scope == ResourceScope::SHARED)
            return 0;
        if (scope == ResourceScope::PER_FRAME)
            return 1;
        throw std::invalid_argument("Unknown resource scope");
    }
    template<typename Schema, typename Bindings>
    consteval size_t TypedBundlePlan<Schema, Bindings>::BindingCount() {
        size_t count = 0;
        for (const auto& entry : Schema::ENTRIES)
            count = detail::allocation_add(count, detail::allocation_multiply(entry.descriptors, entry.allocation == AllocationKind::TEXTURE ? 2 : 1));
        return count;
    }
    template<typename Schema, typename Bindings>
    template<size_t first, size_t second>
    consteval bool TypedBundlePlan<Schema, Bindings>::SamePayload() {
        using A = std::tuple_element_t<first, typename Schema::Allocations>;
        using B = std::tuple_element_t<second, typename Schema::Allocations>;
        return std::is_same_v<typename Schema::template Binding<A::SET, A::BINDING>::Payload, typename Schema::template Binding<B::SET, B::BINDING>::Payload>;
    }
    template<typename Schema, typename Bindings>
    constexpr ResourceId TypedBundlePlan<Schema, Bindings>::Identity(size_t index) {
        auto mapped = [](size_t i, size_t j) { return Bindings::KEYS[i] == KEYS[j].binding && Bindings::COMPONENTS[i] == KEYS[j].component; };
        for (size_t i = 0; i < Bindings::KEYS.size(); ++i)
            if (mapped(i, index))
                return Bindings::RESOURCES[i];
        std::uint64_t id = 0;
        for (size_t current = 0; current <= index; ++current) {
            bool explicit_id = false;
            for (size_t i = 0; i < Bindings::KEYS.size(); ++i)
                explicit_id |= mapped(i, current);
            if (explicit_id)
                continue;
            while (std::ranges::find(Bindings::RESOURCES, ResourceId{id}) != Bindings::RESOURCES.end()) {
                if (id == UINT64_MAX)
                    throw std::overflow_error("Resource ID overflow");
                ++id;
            }
            if (current == index)
                return ResourceId{id};
            if (id == UINT64_MAX)
                throw std::overflow_error("Resource ID overflow");
            ++id;
        }
        throw std::logic_error("Missing resource identity");
    }
    template<typename Schema, typename Bindings>
    consteval auto TypedBundlePlan<Schema, Bindings>::MakeDescriptorIds() {
        std::array<ResourceId, BindingCount()> reserved{};
        for (size_t i = 0; i < reserved.size(); ++i)
            reserved[i] = Identity(i);
        std::array<ResourceId, detail::allocation_add(detail::allocation_multiply(Resources::COUNT, 2), 2 + LayoutShape::NEEDS_EMPTY)> result{};
        std::uint64_t next = 0;
        for (size_t i = 0; i < result.size(); ++i) {
            while (std::ranges::find(reserved, ResourceId{next}) != reserved.end()) {
                if (next == UINT64_MAX)
                    throw std::overflow_error("Descriptor resource ID overflow");
                ++next;
            }
            result[i] = {next};
            if (i + 1 < result.size()) {
                if (next == UINT64_MAX)
                    throw std::overflow_error("Descriptor resource ID overflow");
                ++next;
            }
        }
        return result;
    }
    template<typename Schema, typename Bindings>
    constexpr TypedBundlePlan<Schema, Bindings>::TypedBundlePlan(const ResourceAllocationPlan<Schema>& plan) : _descriptors(plan), _frames(plan.frame_count()) {
        constexpr auto payloads = [] {
            std::array<std::array<bool, Schema::COUNT>, Schema::COUNT> result{};
            [&]<size_t... i>(std::index_sequence<i...>) {
                auto row = [&]<size_t a, size_t... b>(std::index_sequence<b...>) { ((result[a][b] = SamePayload<a, b>()), ...); };
                (row.template operator()<i>(std::make_index_sequence<Schema::COUNT>{}), ...);
            }(std::make_index_sequence<Schema::COUNT>{});
            return result;
        }();
        for (size_t i = 0; i < Bindings::KEYS.size(); ++i)
            if (std::ranges::find(KEYS, detail::BundleBindingKey{Bindings::KEYS[i], Bindings::COMPONENTS[i]}) == KEYS.end())
                throw std::invalid_argument("Unknown bundle binding component");
        for (size_t index = 0; index < KEYS.size(); ++index) {
            const auto key = KEYS[index];
            const auto entry_index = Schema::Index(key.binding.set, key.binding.binding);
            const auto& entry = plan.entries()[entry_index];
            const auto& d = entry.declaration;
            const bool image = key.component == ResourceComponent::IMAGE || d.allocation == AllocationKind::IMAGE ||
                               (d.external && (d.kind == graphic_resource::Type::SAMPLED_IMAGE || d.kind == graphic_resource::Type::STORAGE_IMAGE || d.kind == graphic_resource::Type::COMBINED_IMAGE_SAMPLER));
            const bool sampler = key.component == ResourceComponent::SAMPLER || d.allocation == AllocationKind::SAMPLER || (d.external && d.kind == graphic_resource::Type::SAMPLER);
            const auto scope = image ? d.image_scope : sampler ? d.sampler_scope
                                                               : d.scope;
            const auto kind = image ? storage::ResourceKind::IMAGE : sampler ? storage::ResourceKind::SAMPLER
                                                                             : storage::ResourceKind::BUFFER;
            auto& target = _placements[index];
            target = {key.binding, Identity(index), scope, d.external, SIZE_MAX, key.component, kind, SIZE_MAX};
            BundleResourceInit info;
            if (image)
                info = plan.sizes().image_info(entry_index, key.binding.element);
            else if (sampler)
                info = plan.sizes().sampler_info(entry_index, key.binding.element);
            else
                info = BufferCreateInfo{detail::allocation_multiply(entry.elements, d.payload_size),
                                        (d.kind == graphic_resource::Type::UNIFORM_BUFFER ? MemoryType::UNIFORM_BUFFER : MemoryType::STORAGE_BUFFER) | MemoryType::TRANSFER_DESTINATION,
                                        MemoryProperty::HOST_VISIBLE | MemoryProperty::HOST_COHERENT};
            for (size_t previous = 0; previous < index; ++previous) {
                const auto& other = _placements[previous];
                if (other.resource != target.resource)
                    continue;
                const auto other_index = Schema::Index(other.binding.set, other.binding.binding);
                const auto& od = plan.entries()[other_index].declaration;
                if (target.external || other.external || target.scope != other.scope || target.kind != other.kind ||
                    (kind == storage::ResourceKind::BUFFER && (d.kind != od.kind || !payloads[entry_index][other_index])) ||
                    (image && (d.kind == graphic_resource::Type::STORAGE_IMAGE) != (od.kind == graphic_resource::Type::STORAGE_IMAGE)) ||
                    !detail::bundle_info_equal(info, _infos[ScopeIndex(other.scope)][other.slot]))
                    throw std::invalid_argument("Incompatible declarations for one bundle resource ID");
                target.slot = other.slot;
                break;
            }
            if (target.external || target.slot != SIZE_MAX)
                continue;
            const auto group = ScopeIndex(scope);
            target.slot = _counts[group]++;
            _requests[group][target.slot] = {target.resource, kind};
            _infos[group][target.slot] = info;
            if (kind == storage::ResourceKind::BUFFER)
                _buffers[group][_buffer_counts[group]++] = std::get<BufferCreateInfo>(info);
        }
        for (auto& target : _placements)
            if (!target.external)
                target.offset = resource_plan(target.scope).placement(target.resource).offset;
        static_cast<void>(total_bytes());
        static_cast<void>(total_arena_bytes());
    }
    template<typename Schema, typename Bindings>
    constexpr ResourcePlan<TypedBundlePlan<Schema, Bindings>::CAPACITY> TypedBundlePlan<Schema, Bindings>::resource_plan(ResourceScope scope, size_t instance) const {
        const auto group = ScopeIndex(scope);
        return ResourcePlan<CAPACITY>(std::span(_requests[group].data(), _counts[group]), {scope == ResourceScope::SHARED ? LifetimeScope::SHARED : LifetimeScope::FRAME, instance});
    }
    template<typename Schema, typename Bindings>
    constexpr ResourceId TypedBundlePlan<Schema, Bindings>::descriptor_layout(std::uint32_t set) const {
        const auto layouts = TypedDescriptorPlan<Schema>::Layouts();
        for (size_t i = 0; i < layouts.size(); ++i)
            if (layouts[i].set == set)
                return DESCRIPTOR_IDS[i];
        throw std::out_of_range("Unknown bundle descriptor layout");
    }
    template<typename Schema, typename Bindings>
    constexpr ResourceId TypedBundlePlan<Schema, Bindings>::descriptor_set(std::uint32_t set) const {
        const auto layouts = TypedDescriptorPlan<Schema>::Layouts();
        for (size_t i = 0; i < layouts.size(); ++i)
            if (layouts[i].set == set)
                return DESCRIPTOR_IDS[Resources::COUNT + i];
        throw std::out_of_range("Unknown bundle descriptor set");
    }
    template<typename Schema, typename Bindings>
    constexpr std::optional<ResourceId> TypedBundlePlan<Schema, Bindings>::empty_descriptor_layout() const {
        if constexpr (NEEDS_EMPTY_LAYOUT)
            return DESCRIPTOR_IDS[Resources::COUNT * 2 + 2];
        else if constexpr (LayoutShape::HAS_HOLES)
            return DESCRIPTOR_IDS[LayoutShape::EMPTY_INDEX];
        return std::nullopt;
    }
    template<typename Schema, typename Bindings>
    constexpr std::optional<ResourceId> TypedBundlePlan<Schema, Bindings>::descriptor_pool(ResourceScope scope) const {
        const auto group = ScopeIndex(scope);
        if (!_descriptors.pool(scope).max_sets)
            return std::nullopt;
        return DESCRIPTOR_IDS[Resources::COUNT * 2 + group];
    }
    template<typename Schema, typename Bindings>
    constexpr ResourcePlan<TypedBundlePlan<Schema, Bindings>::ARENA_CAPACITY> TypedBundlePlan<Schema, Bindings>::arena_plan(ResourceScope scope, size_t instance) const {
        const auto group = ScopeIndex(scope);
        std::array<ResourceRequest, ARENA_CAPACITY> requests{};
        size_t count = _counts[group];
        std::copy_n(_requests[group].begin(), count, requests.begin());
        if (scope == ResourceScope::SHARED) {
            for (const auto& layout : TypedDescriptorPlan<Schema>::Layouts())
                requests[count++] = {descriptor_layout(layout.set), storage::ResourceKind::DESCRIPTOR_LAYOUT};
        } else {
            for (const auto& set : _descriptors.sets(scope))
                requests[count++] = {descriptor_layout(set.set), storage::ResourceKind::DESCRIPTOR_LAYOUT, true};
        }
        if (const auto pool = descriptor_pool(scope)) {
            requests[count++] = {*pool, storage::ResourceKind::DESCRIPTOR_POOL};
            for (const auto& set : _descriptors.sets(scope))
                requests[count++] = {descriptor_set(set.set), storage::ResourceKind::DESCRIPTOR_SET};
        }
        if constexpr (NEEDS_EMPTY_LAYOUT)
            if (scope == ResourceScope::SHARED)
                requests[count++] = {*empty_descriptor_layout(), storage::ResourceKind::DESCRIPTOR_LAYOUT};
        return ResourcePlan<ARENA_CAPACITY>(std::span(requests.data(), count), {scope == ResourceScope::SHARED ? LifetimeScope::SHARED : LifetimeScope::FRAME, instance});
    }
    template<typename Schema, typename Bindings>
    constexpr BundleResourcePlacement TypedBundlePlan<Schema, Bindings>::placement(BindingKey key, ResourceComponent component) const {
        for (const auto& entry : _placements)
            if (entry.binding == key && entry.component == component)
                return entry;
        throw std::invalid_argument("Unknown bundle binding component");
    }
} // namespace VSL_NAMESPACE::typed
