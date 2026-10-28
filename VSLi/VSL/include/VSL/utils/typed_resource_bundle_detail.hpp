#pragma once
#include "typed_resource_allocation.hpp"
#include "typed_resource_storage.hpp"
#include "typed_frame_resources.hpp"
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
        template<typename Schema, typename Spec, typename Policy, bool external = Spec::EXTERNAL>
        struct BundleValue {
            using Binding = typename Schema::template Binding<Spec::SET, Spec::BINDING>;
            static_assert(storage_policy_binding_matches<Policy, Binding, Spec>(), "Invalid typed resource storage policy");
            using Element = typename Policy::template Buffer<Binding, Spec>;
            using Type = std::array<Element, Binding::POINT.amount>;
        };
        template<typename Schema, typename Spec, typename Policy>
        struct BundleValue<Schema, Spec, Policy, true> {
            using Binding = typename Schema::template Binding<Spec::SET, Spec::BINDING>;
            using Element = BundleExternalValue<Binding>;
            using Type = std::array<Element, Binding::POINT.amount>;
        };
        template<typename Schema, typename Tuple, typename Policy>
        struct BundleStorage;
        template<typename Schema, typename Policy, typename... Specs>
        struct BundleStorage<Schema, std::tuple<Specs...>, Policy> {
            using Type = std::tuple<std::optional<typename BundleValue<Schema, Specs, Policy>::Type>...>;
        };
        template<typename Schema, typename Policy>
        using BundleStorageType = typename BundleStorage<Schema, typename Schema::Allocations, Policy>::Type;
        template<typename Tuple>
        struct BundleSets;
        template<typename... Sets>
        struct BundleSets<std::tuple<Sets...>> {
            using Type = std::tuple<TypedDescriptorSet<typename Sets::Layout>...>;
            using Optional = std::tuple<std::optional<TypedDescriptorSet<typename Sets::Layout>>...>;
        };
        template<typename Schema, typename Policy>
        struct BundleSharedState {
            BundleStorageType<Schema, Policy> resources;
            typename BundleSets<typename Schema::Resources::Sets>::Optional sets;
        };
        template<typename Schema, typename Spec, typename Policy>
        auto make_bundle_buffer(LogicalDeviceAccessor device, Policy& policy, size_t count, size_t element);
        template<typename Schema, typename Spec, typename Policy, typename Inputs>
        auto make_bundle_values(LogicalDeviceAccessor device, const ResourceAllocationPlan<Schema>& plan, Policy& policy, const Inputs& inputs);
        template<ResourceScope scope, typename Schema, typename Policy, typename Inputs>
        auto make_bundle_storage(LogicalDeviceAccessor device, const ResourceAllocationPlan<Schema>& plan, Policy& policy, const Inputs& inputs);
        template<typename Schema, typename Spec, typename Policy>
        const auto& bundle_values(const BundleStorageType<Schema, Policy>& frame, const BundleSharedState<Schema, Policy>& shared);
        template<typename Schema, typename Set, typename Policy>
        void connect_bundle_set(TypedDescriptorSet<typename Set::Layout>& set, const BundleStorageType<Schema, Policy>& frame, const BundleSharedState<Schema, Policy>& shared);
        template<typename Schema, typename Set, typename Policy>
        auto make_bundle_set(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager, const BundleStorageType<Schema, Policy>& frame, const BundleSharedState<Schema, Policy>& shared);
        template<typename Schema, typename Policy>
        auto make_bundle_sets(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager, const BundleStorageType<Schema, Policy>& frame, const BundleSharedState<Schema, Policy>& shared);
        template<typename Schema, typename Policy, typename Inputs>
        auto make_bundle_shared(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager, const ResourceAllocationPlan<Schema>& plan, Policy& policy, const Inputs& inputs);
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
        template<typename Schema, typename Spec, typename Policy>
        auto make_bundle_buffer(LogicalDeviceAccessor device, Policy& policy, size_t count, size_t element) {
            using Binding = typename Schema::template Binding<Spec::SET, Spec::BINDING>;
            auto value = policy.template make_buffer<Binding, Spec>(device, count, element);
            validate_storage_policy_buffer(value.descriptor(), value.size(), count, sizeof(typename Binding::Payload));
            return value;
        }
        template<typename Schema, typename Spec, typename Policy, typename Inputs>
        auto make_bundle_values(LogicalDeviceAccessor device, const ResourceAllocationPlan<Schema>& plan, Policy& policy, const Inputs& inputs) {
            using Value = BundleValue<Schema, Spec, Policy>;
            return [&]<size_t... indices>(std::index_sequence<indices...>) {
                if constexpr (Spec::EXTERNAL) {
                    const auto& input = std::get<bundle_input_index<Inputs>(Spec::SET, Spec::BINDING)>(inputs);
                    return typename Value::Type{{typename Value::Element{input.values[indices].descriptor()}...}};
                } else {
                    const auto count = plan.template entry<Spec::SET, Spec::BINDING>().elements;
                    return typename Value::Type{{make_bundle_buffer<Schema, Spec>(device, policy, count, indices)...}};
                }
            }(std::make_index_sequence<Value::Binding::POINT.amount>{});
        }
        template<ResourceScope scope, typename Schema, typename Policy, typename Inputs>
        auto make_bundle_storage(LogicalDeviceAccessor device, const ResourceAllocationPlan<Schema>& plan, Policy& policy, const Inputs& inputs) {
            BundleStorageType<Schema, Policy> storage;
            bundle_for_each<typename Schema::Allocations>([&]<typename Spec>() {
                if constexpr (Spec::SCOPE == scope)
                    std::get<Schema::Index(Spec::SET, Spec::BINDING)>(storage).emplace(make_bundle_values<Schema, Spec>(device, plan, policy, inputs));
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
                const auto& values = bundle_values<Schema, Spec>(frame, shared);
                if constexpr (Binding::POINT.amount == 1)
                    set.template write<Binding::POINT.binding>(values[0]);
                else
                    set.template write<Binding::POINT.binding>(values);
            });
        }
        template<typename Schema, typename Set, typename Policy>
        auto make_bundle_set(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager, const BundleStorageType<Schema, Policy>& frame, const BundleSharedState<Schema, Policy>& shared) {
            if constexpr (Schema::SetIsPerFrame(Set::SET)) {
                TypedDescriptorSet<typename Set::Layout> result(device, manager);
                connect_bundle_set<Schema, Set, Policy>(result, frame, shared);
                return result;
            } else
                return std::get<Schema::SetIndex(Set::SET)>(shared.sets).value();
        }
        template<typename Schema, typename Policy>
        auto make_bundle_sets(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager, const BundleStorageType<Schema, Policy>& frame, const BundleSharedState<Schema, Policy>& shared) {
            return [&]<size_t... indices>(std::index_sequence<indices...>) {
                return typename BundleSets<typename Schema::Resources::Sets>::Type{make_bundle_set<Schema, typename Schema::Resources::template Set<indices>, Policy>(device, manager, frame, shared)...};
            }(std::make_index_sequence<Schema::Resources::COUNT>{});
        }
        template<typename Schema, typename Policy, typename Inputs>
        auto make_bundle_shared(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager, const ResourceAllocationPlan<Schema>& plan, Policy& policy, const Inputs& inputs) {
            validate_bundle_plan(device, manager, plan.entries());
            auto shared = std::make_shared<BundleSharedState<Schema, Policy>>(make_bundle_storage<ResourceScope::SHARED>(device, plan, policy, inputs));
            bundle_for_each<typename Schema::Resources::Sets>([&]<typename Set>() {
                if constexpr (!Schema::SetIsPerFrame(Set::SET)) {
                    auto& target = std::get<Schema::SetIndex(Set::SET)>(shared->sets);
                    target.emplace(device, manager);
                    connect_bundle_set<Schema, Set, Policy>(*target, shared->resources, *shared);
                }
            });
            return shared;
        }
    } // namespace detail
} // namespace VSL_NAMESPACE::typed
