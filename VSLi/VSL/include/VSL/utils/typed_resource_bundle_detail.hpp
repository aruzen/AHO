#pragma once
#include "typed_resource_allocation.hpp"
#include "typed_resources.hpp"
#include "typed_frame_resources.hpp"
#include "../lifetime/typed_bundle_plan.hpp"
#include "../lifetime/typed_resource_buffer_view.hpp"
#include "../lifetime/resource_arena.hpp"
#include <optional>

namespace VSL_NAMESPACE::typed {
    template<std::uint32_t set, std::uint32_t binding, typename Value_, size_t count>
    struct ResourceInput {
        using Value = Value_;
        static constexpr std::uint32_t SET = set, BINDING = binding;
        static constexpr size_t COUNT = count;
        std::array<Value, count> values;
    };
    template<std::uint32_t set, std::uint32_t binding, typename Value>
    auto external_resource(const Value& value);
    template<std::uint32_t set, std::uint32_t binding, typename Value, size_t count>
    auto external_resource(const std::array<Value, count>& values);

    namespace detail {
        template<typename Policy>
        struct BundlePolicy;
        template<>
        struct BundlePolicy<HeapStorage> {
            static constexpr bool ARENA = false;
            static constexpr auto RETENTION = ResourceRetention::SUBMISSION;
            using Bindings = BufferBindingMap<>;
        };
        template<typename Map>
        struct BundlePolicy<ArenaStorage<Map>> {
            static constexpr bool ARENA = true;
            static constexpr auto RETENTION = ResourceRetention::COMPLETION;
            using Bindings = Map;
        };
        template<typename Schema, typename Policy>
        using BundlePlacementPlan = TypedBundlePlan<Schema, typename BundlePolicy<Policy>::Bindings>;
        template<ResourceScope scope, typename Schema, typename Policy>
        std::optional<lifetime::ResourceArena> make_bundle_arena(LogicalDeviceAccessor device, const ResourceAllocationPlan<Schema>& plan, size_t instance = 0, const lifetime::ResourceArena* shared = nullptr);
        DescriptorLayoutCreateInfo make_bundle_layout(const DescriptorLayoutPlan& plan);
        DescriptorPoolCreateInfo make_bundle_pool(DescriptorPoolPlan plan);
        struct BundleNativeResources {
            std::vector<std::pair<ResourceId, ImageAccessor>> images;
            std::vector<std::pair<ResourceId, SamplerAccessor>> samplers;
            ImageAccessor image(ResourceId id) const;
            SamplerAccessor sampler(ResourceId id) const;
            void require_idle() const;
        };
        BundleNativeResources make_bundle_native(LogicalDeviceAccessor device, std::span<const ResourcePlacement> placements, std::span<const BundleResourceInit> infos);
        template<ResourceScope scope, typename Schema, typename Policy>
        BundleNativeResources make_bundle_native(LogicalDeviceAccessor device, const ResourceAllocationPlan<Schema>& plan);
        size_t bundle_frame_count(CommandManager manager);
        void validate_bundle_plan(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager, std::span<const ResourceAllocationRecord> entries);
        template<typename T>
        struct IsResourceInput : std::false_type {};
        template<std::uint32_t set, std::uint32_t binding, typename Value, size_t count>
        struct IsResourceInput<ResourceInput<set, binding, Value, count>> : std::true_type {};
        template<typename Schema, typename... Inputs>
        consteval bool bundle_inputs_match();
        template<typename Tuple, typename Function>
        void bundle_for_each(Function&& function);
        template<typename Tuple>
        consteval size_t bundle_input_index(std::uint32_t set, std::uint32_t binding);

        template<typename Binding>
        struct BundleExternalValue {
            using Payload = typename Binding::Payload;
            static constexpr auto KIND = Binding::POINT.binding_type;
            DescriptorValue value;
            DescriptorValue descriptor() const { return value; }
        };
        template<typename Binding, typename Policy, bool buffer = buffer_descriptor(Binding::POINT.binding_type)>
        struct BundleOwnedValue {
            using Type = TypedImageResource<Binding::POINT.binding_type>;
        };
        template<typename Binding, typename Policy>
        struct BundleOwnedValue<Binding, Policy, true> {
            using Type = std::conditional_t<BundlePolicy<Policy>::ARENA, PlacedResourceBufferView<typename Binding::Payload, Binding::POINT.binding_type>, TypedResourceBuffer<typename Binding::Payload, Binding::POINT.binding_type>>;
        };
        template<typename Schema, typename Spec, typename Policy, bool external = Spec::EXTERNAL>
        struct BundleValue {
            using Binding = typename Schema::template Binding<Spec::SET, Spec::BINDING>;
            using Element = typename BundleOwnedValue<Binding, Policy>::Type;
            using Type = std::array<Element, Binding::POINT.amount>;
        };
        template<typename Schema, typename Spec, typename Policy>
        struct BundleValue<Schema, Spec, Policy, true> {
            using Binding = typename Schema::template Binding<Spec::SET, Spec::BINDING>;
            using Element = BundleExternalValue<Binding>;
            using Type = std::array<Element, Binding::POINT.amount>;
        };
        template<typename Schema, typename Policy, typename Tuple>
        struct BundleStorage;
        template<typename Schema, typename Policy, typename... Specs>
        struct BundleStorage<Schema, Policy, std::tuple<Specs...>> {
            using Type = std::tuple<std::optional<typename BundleValue<Schema, Specs, Policy>::Type>...>;
        };
        template<typename Schema, typename Policy>
        using BundleStorageType = typename BundleStorage<Schema, Policy, typename Schema::Allocations>::Type;
        template<typename Tuple>
        struct BundleSets;
        template<typename... Sets>
        struct BundleSets<std::tuple<Sets...>> {
            using Type = std::tuple<TypedDescriptorSet<typename Sets::Layout>...>;
            using Optional = std::tuple<std::optional<TypedDescriptorSet<typename Sets::Layout>>...>;
        };
        template<typename Schema, typename Policy>
        struct BundleSharedState {
            BundlePlacementPlan<Schema, Policy> layout;
            std::optional<lifetime::ResourceArena> arena;
            BundleNativeResources native;
            BundleStorageType<Schema, Policy> resources;
            typename BundleSets<typename Schema::Resources::Sets>::Optional sets;
            explicit BundleSharedState(const ResourceAllocationPlan<Schema>& plan) : layout(plan) {}
        };
        template<typename Schema, typename Spec, typename Policy, typename Inputs>
        auto make_bundle_values(LogicalDeviceAccessor device, const ResourceAllocationPlan<Schema>& plan, const Inputs& inputs, const lifetime::ResourceArena* arena, const BundleNativeResources& native, const BundleSharedState<Schema, Policy>& shared);
        template<ResourceScope scope, typename Schema, typename Policy, typename Inputs>
        auto make_bundle_storage(LogicalDeviceAccessor device, const ResourceAllocationPlan<Schema>& plan, const Inputs& inputs, const lifetime::ResourceArena* arena, const BundleNativeResources& native, const BundleSharedState<Schema, Policy>& shared);
        template<typename Schema, typename Policy>
        ImageAccessor bundle_image(const BundleSharedState<Schema, Policy>& shared, const lifetime::ResourceArena* arena, const BundleNativeResources& native, const BundleResourcePlacement& placement);
        template<typename Schema, typename Policy>
        SamplerAccessor bundle_sampler(const BundleSharedState<Schema, Policy>& shared, const lifetime::ResourceArena* arena, const BundleNativeResources& native, const BundleResourcePlacement& placement);
        template<typename Schema, typename Spec, typename Policy>
        const auto& bundle_values(const BundleStorageType<Schema, Policy>& frame, const BundleSharedState<Schema, Policy>& shared);
        template<typename Schema, typename Set, typename Policy>
        void connect_bundle_set(TypedDescriptorSet<typename Set::Layout>& set, const BundleStorageType<Schema, Policy>& frame, const BundleSharedState<Schema, Policy>& shared);
        template<typename Schema, typename Set, typename Policy>
        auto allocate_bundle_set(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager, const BundlePlacementPlan<Schema, Policy>& layout, const lifetime::ResourceArena* arena);
        template<typename Schema, typename Set, typename Policy>
        auto make_bundle_set(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager, const BundleStorageType<Schema, Policy>& frame, const BundleSharedState<Schema, Policy>& shared, const lifetime::ResourceArena* arena);
        template<typename Schema, typename Policy>
        auto make_bundle_sets(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager, const BundleStorageType<Schema, Policy>& frame, const BundleSharedState<Schema, Policy>& shared, const lifetime::ResourceArena* arena);
        template<typename Schema, typename Policy, typename Inputs>
        auto make_bundle_shared(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager, const ResourceAllocationPlan<Schema>& plan, const Inputs& inputs);
        template<typename Schema, typename Policy>
        void require_bundle_shared_idle(const BundleSharedState<Schema, Policy>& shared);
    } // namespace detail

    template<std::uint32_t set, std::uint32_t binding, typename Value>
    auto external_resource(const Value& value) {
        return ResourceInput<set, binding, Value, 1>{{value}};
    }
    template<std::uint32_t set, std::uint32_t binding, typename Value, size_t count>
    auto external_resource(const std::array<Value, count>& values) {
        return ResourceInput<set, binding, Value, count>{values};
    }

    namespace detail {
        template<ResourceScope scope, typename Schema, typename Policy>
        std::optional<lifetime::ResourceArena> make_bundle_arena(LogicalDeviceAccessor device, const ResourceAllocationPlan<Schema>& plan, size_t instance, const lifetime::ResourceArena* shared) {
            if constexpr (BundlePolicy<Policy>::ARENA) {
                const BundlePlacementPlan<Schema, Policy> layout(plan);
                const auto physical = layout.arena_plan(scope, instance);
                if (physical.size()) {
                    std::vector<lifetime::ResourceInit> infos;
                    infos.reserve(physical.size());
                    for (const auto& info : layout.resources(scope))
                        std::visit([&](const auto& value) { infos.emplace_back(value); }, info);
                    if constexpr (scope == ResourceScope::SHARED) {
                        for (const auto& declaration : layout.descriptors().Layouts())
                            infos.emplace_back(make_bundle_layout(declaration));
                    } else {
                        for (const auto& set : layout.descriptors().sets(scope)) {
                            if (!shared)
                                throw std::logic_error("Frame descriptor layouts require the shared arena");
                            infos.emplace_back(shared->descriptor_layout(layout.descriptor_layout(set.set)));
                        }
                    }
                    if (const auto pool = layout.descriptor_pool(scope)) {
                        infos.emplace_back(make_bundle_pool(layout.descriptors().pool(scope)));
                        for (const auto& set : layout.descriptors().sets(scope))
                            infos.emplace_back(lifetime::DescriptorSetInit{*pool, layout.descriptor_layout(set.set)});
                    }
                    if constexpr (scope == ResourceScope::SHARED && BundlePlacementPlan<Schema, Policy>::NEEDS_EMPTY_LAYOUT)
                        infos.emplace_back(DescriptorLayoutCreateInfo{});
                    lifetime::ResourceArena arena(device, physical, infos, lifetime::DependencyPolicy::DYNAMIC);
                    for (const auto& slot : physical.placements())
                        if (slot.kind == storage::ResourceKind::BUFFER)
                            arena.borrow_buffer(slot.resource).fill(std::byte{});
                    return arena;
                }
            }
            return std::nullopt;
        }
        template<ResourceScope scope, typename Schema, typename Policy>
        BundleNativeResources make_bundle_native(LogicalDeviceAccessor device, const ResourceAllocationPlan<Schema>& plan) {
            if constexpr (!BundlePolicy<Policy>::ARENA) {
                const BundlePlacementPlan<Schema, Policy> layout(plan);
                const auto physical = layout.resource_plan(scope);
                return make_bundle_native(device, physical.placements(), layout.resources(scope));
            }
            return {};
        }
        template<typename Schema, typename Policy>
        ImageAccessor bundle_image(const BundleSharedState<Schema, Policy>& shared, const lifetime::ResourceArena* arena, const BundleNativeResources& native, const BundleResourcePlacement& placement) {
            const bool is_shared = placement.scope == ResourceScope::SHARED;
            if constexpr (BundlePolicy<Policy>::ARENA)
                return (is_shared ? &shared.arena.value() : arena)->image(placement.resource);
            else
                return (is_shared ? shared.native : native).image(placement.resource);
        }
        template<typename Schema, typename Policy>
        SamplerAccessor bundle_sampler(const BundleSharedState<Schema, Policy>& shared, const lifetime::ResourceArena* arena, const BundleNativeResources& native, const BundleResourcePlacement& placement) {
            const bool is_shared = placement.scope == ResourceScope::SHARED;
            if constexpr (BundlePolicy<Policy>::ARENA)
                return (is_shared ? &shared.arena.value() : arena)->sampler(placement.resource);
            else
                return (is_shared ? shared.native : native).sampler(placement.resource);
        }
        template<typename Schema, typename... Inputs>
        consteval bool bundle_inputs_match() {
            if constexpr (!(IsResourceInput<Inputs>::value && ...))
                return false;
            else {
                size_t expected = 0;
                for (const auto& entry : Schema::ENTRIES)
                    expected += entry.external;
                if (expected != sizeof...(Inputs))
                    return false;
                constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, sizeof...(Inputs)> keys{{{Inputs::SET, Inputs::BINDING}...}};
                for (size_t i = 0; i < keys.size(); ++i)
                    for (size_t j = 0; j < i; ++j)
                        if (keys[i] == keys[j])
                            return false;
                return ([]<typename Input>() {
                    using Spec = typename Schema::template Spec<Input::SET, Input::BINDING>;
                    using Binding = typename Schema::template Binding<Input::SET, Input::BINDING>;
                    if constexpr (std::is_void_v<Spec> || std::is_void_v<Binding>)
                        return false;
                    else
                        return Spec::EXTERNAL && descriptor_matches<Binding, typename Input::Value>() && Binding::POINT.amount == Input::COUNT;
                }.template operator()<Inputs>() &&
                        ...);
            }
        }
        template<typename Tuple, typename Function>
        void bundle_for_each(Function&& function) {
            [&]<size_t... indices>(std::index_sequence<indices...>) {
                (function.template operator()<std::tuple_element_t<indices, Tuple>>(), ...);
            }(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
        }
        template<typename Tuple>
        consteval size_t bundle_input_index(std::uint32_t set, std::uint32_t binding) {
            return [&]<size_t... indices>(std::index_sequence<indices...>) {
                constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, sizeof...(indices)> keys{{{std::remove_cvref_t<std::tuple_element_t<indices, Tuple>>::SET, std::remove_cvref_t<std::tuple_element_t<indices, Tuple>>::BINDING}...}};
                for (size_t index = 0; index < keys.size(); ++index)
                    if (keys[index] == std::pair{set, binding})
                        return index;
                throw "Missing typed allocation input";
            }(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
        }
        template<typename Schema, typename Spec, typename Policy, typename Inputs>
        auto make_bundle_values(LogicalDeviceAccessor device, const ResourceAllocationPlan<Schema>& plan, const Inputs& inputs, const lifetime::ResourceArena* arena, const BundleNativeResources& native, const BundleSharedState<Schema, Policy>& shared) {
            using Value = BundleValue<Schema, Spec, Policy>;
            return [&]<size_t... indices>(std::index_sequence<indices...>) {
                if constexpr (Spec::EXTERNAL) {
                    const auto& input = std::get<bundle_input_index<Inputs>(Spec::SET, Spec::BINDING)>(inputs);
                    return typename Value::Type{{typename Value::Element{input.values[indices].descriptor()}...}};
                } else {
                    auto make = [&]<size_t index>() -> typename Value::Element {
                        constexpr auto kind = Value::Binding::POINT.binding_type;
                        constexpr BindingKey key{Spec::SET, Spec::BINDING, index};
                        if constexpr (buffer_descriptor(kind)) {
                            if constexpr (BundlePolicy<Policy>::ARENA)
                                return typename Value::Element(arena->buffer(shared.layout.placement(key).resource));
                            else
                                return typename Value::Element(device, std::in_place, plan.template entry<Spec::SET, Spec::BINDING>().elements);
                        } else if constexpr (kind == graphic_resource::Type::COMBINED_IMAGE_SAMPLER)
                            return typename Value::Element(bundle_image(shared, arena, native, shared.layout.placement(key, ResourceComponent::IMAGE)), bundle_sampler(shared, arena, native, shared.layout.placement(key, ResourceComponent::SAMPLER)));
                        else if constexpr (kind == graphic_resource::Type::SAMPLER)
                            return typename Value::Element(bundle_sampler(shared, arena, native, shared.layout.placement(key)));
                        else
                            return typename Value::Element(bundle_image(shared, arena, native, shared.layout.placement(key)));
                    };
                    return typename Value::Type { {make.template operator()<indices>()...} };
                }
            }(std::make_index_sequence<Value::Binding::POINT.amount>{});
        }
        template<ResourceScope scope, typename Schema, typename Policy, typename Inputs>
        auto make_bundle_storage(LogicalDeviceAccessor device, const ResourceAllocationPlan<Schema>& plan, const Inputs& inputs, const lifetime::ResourceArena* arena, const BundleNativeResources& native, const BundleSharedState<Schema, Policy>& shared) {
            BundleStorageType<Schema, Policy> storage;
            bundle_for_each<typename Schema::Allocations>([&]<typename Spec>() {
                if constexpr (Spec::SCOPE == scope)
                    std::get<Schema::Index(Spec::SET, Spec::BINDING)>(storage).emplace(make_bundle_values<Schema, Spec, Policy>(device, plan, inputs, arena, native, shared));
            });
            return storage;
        }
        template<typename Schema, typename Spec, typename Policy>
        const auto& bundle_values(const BundleStorageType<Schema, Policy>& frame, const BundleSharedState<Schema, Policy>& shared) {
            if constexpr (Spec::SCOPE == ResourceScope::PER_FRAME)
                return std::get<Schema::Index(Spec::SET, Spec::BINDING)>(frame).value();
            else
                return std::get<Schema::Index(Spec::SET, Spec::BINDING)>(shared.resources).value();
        }
        template<typename Schema, typename Set, typename Policy>
        void connect_bundle_set(TypedDescriptorSet<typename Set::Layout>& set, const BundleStorageType<Schema, Policy>& frame, const BundleSharedState<Schema, Policy>& shared) {
            bundle_for_each<typename Set::Bindings>([&]<typename Binding>() {
                using Spec = typename Schema::template Spec<Set::SET, Binding::POINT.binding>;
                const auto& values = bundle_values<Schema, Spec, Policy>(frame, shared);
                if constexpr (Binding::POINT.amount == 1)
                    set.template write<Binding::POINT.binding>(values[0]);
                else
                    set.template write<Binding::POINT.binding>(values);
            });
        }
        template<typename Schema, typename Set, typename Policy>
        auto allocate_bundle_set(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager, const BundlePlacementPlan<Schema, Policy>& layout, const lifetime::ResourceArena* arena) {
            using Result = TypedDescriptorSet<typename Set::Layout>;
            if constexpr (BundlePolicy<Policy>::ARENA) {
                if (!arena)
                    throw std::logic_error("Typed descriptor requires its resource arena");
                return Result::FromResource(device, arena->descriptor_set(layout.descriptor_set(Set::SET)), BundlePolicy<Policy>::RETENTION);
            } else
                return Result(device, manager, BundlePolicy<Policy>::RETENTION);
        }
        template<typename Schema, typename Set, typename Policy>
        auto make_bundle_set(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager, const BundleStorageType<Schema, Policy>& frame, const BundleSharedState<Schema, Policy>& shared, const lifetime::ResourceArena* arena) {
            if constexpr (Schema::SetIsPerFrame(Set::SET)) {
                auto result = allocate_bundle_set<Schema, Set, Policy>(device, manager, shared.layout, arena);
                connect_bundle_set<Schema, Set, Policy>(result, frame, shared);
                return result;
            } else
                return std::get<Schema::SetIndex(Set::SET)>(shared.sets).value();
        }
        template<typename Schema, typename Policy>
        auto make_bundle_sets(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager, const BundleStorageType<Schema, Policy>& frame, const BundleSharedState<Schema, Policy>& shared, const lifetime::ResourceArena* arena) {
            return [&]<size_t... indices>(std::index_sequence<indices...>) {
                return typename BundleSets<typename Schema::Resources::Sets>::Type{make_bundle_set<Schema, typename Schema::Resources::template Set<indices>, Policy>(device, manager, frame, shared, arena)...};
            }(std::make_index_sequence<Schema::Resources::COUNT>{});
        }
        template<typename Schema, typename Policy, typename Inputs>
        auto make_bundle_shared(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager, const ResourceAllocationPlan<Schema>& plan, const Inputs& inputs) {
            validate_bundle_plan(device, manager, plan.entries());
            auto shared = std::make_shared<BundleSharedState<Schema, Policy>>(plan);
            shared->arena = make_bundle_arena<ResourceScope::SHARED, Schema, Policy>(device, plan);
            shared->native = make_bundle_native<ResourceScope::SHARED, Schema, Policy>(device, plan);
            shared->resources = make_bundle_storage<ResourceScope::SHARED, Schema, Policy>(device, plan, inputs, shared->arena ? &*shared->arena : nullptr, shared->native, *shared);
            bundle_for_each<typename Schema::Resources::Sets>([&]<typename Set>() {
                if constexpr (!Schema::SetIsPerFrame(Set::SET)) {
                    auto& target = std::get<Schema::SetIndex(Set::SET)>(shared->sets);
                    target.emplace(allocate_bundle_set<Schema, Set, Policy>(device, manager, shared->layout, shared->arena ? &*shared->arena : nullptr));
                    connect_bundle_set<Schema, Set, Policy>(*target, shared->resources, *shared);
                }
            });
            return shared;
        }
        template<typename Schema, typename Policy>
        void require_bundle_shared_idle(const BundleSharedState<Schema, Policy>& shared) {
            if constexpr (BundlePolicy<Policy>::ARENA) {
                const auto plan = shared.layout.resource_plan(ResourceScope::SHARED);
                for (const auto& entry : plan.placements()) {
                    switch (entry.kind) {
                        case storage::ResourceKind::BUFFER:
                            shared.arena->borrow_buffer(entry.resource).use().require_idle();
                            break;
                        case storage::ResourceKind::IMAGE:
                            shared.arena->image(entry.resource).use_state().require_idle();
                            break;
                        case storage::ResourceKind::SAMPLER:
                            shared.arena->sampler(entry.resource).use_state().require_idle();
                            break;
                        case storage::ResourceKind::DESCRIPTOR_LAYOUT:
                        case storage::ResourceKind::DESCRIPTOR_POOL:
                        case storage::ResourceKind::DESCRIPTOR_SET:
                        case storage::ResourceKind::SHADER:
                        case storage::ResourceKind::PIPELINE_LAYOUT:
                        case storage::ResourceKind::RENDER_PASS:
                        case storage::ResourceKind::PIPELINE:
                        case storage::ResourceKind::FRAME_BUFFER:
                        case storage::ResourceKind::COMMAND_POOL:
                        case storage::ResourceKind::COMMAND_BUFFER:
                        case storage::ResourceKind::SEMAPHORE:
                        case storage::ResourceKind::FENCE:
                            throw std::logic_error("Unexpected descriptor in the data-only bundle plan");
                    }
                }
            } else {
                shared.native.require_idle();
                bundle_for_each<typename Schema::Allocations>([&]<typename Spec>() {
                    if constexpr (!Spec::EXTERNAL && Spec::SCOPE == ResourceScope::SHARED && buffer_descriptor(Schema::template Binding<Spec::SET, Spec::BINDING>::POINT.binding_type))
                        for (const auto& value : std::get<Schema::Index(Spec::SET, Spec::BINDING)>(shared.resources).value())
                            value.descriptor().use.state->require_idle();
                });
            }
            bundle_for_each<typename Schema::Resources::Sets>([&]<typename Set>() {
                if constexpr (!Schema::SetIsPerFrame(Set::SET))
                    std::get<Schema::SetIndex(Set::SET)>(shared.sets).value().resource_use()->require_idle();
            });
        }
    } // namespace detail
} // namespace VSL_NAMESPACE::typed
