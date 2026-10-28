#pragma once
#include "typed_resource_bundle_detail.hpp"

namespace VSL_NAMESPACE::typed {
    template<typename Schema, typename Policy = DefaultResourceStoragePolicy>
    class TypedResourceBundle;

    template<typename Schema, typename Policy = DefaultResourceStoragePolicy>
    class TypedResourceFrame {
        std::shared_ptr<detail::BundleSharedState<Schema, Policy>> _shared;
        detail::BundleStorageType<Schema, Policy> _resources;
        typename detail::BundleSets<typename Schema::Resources::Sets>::Type _sets;
        friend class TypedResourceBundle<Schema, Policy>;
        TypedResourceFrame(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager,
                           std::shared_ptr<detail::BundleSharedState<Schema, Policy>> shared, const ResourceAllocationPlan<Schema>& plan, Policy& policy);

    public:
        TypedResourceFrame(const TypedResourceFrame&) = delete;
        TypedResourceFrame& operator=(const TypedResourceFrame&) = delete;
        TypedResourceFrame(TypedResourceFrame&&) = default;
        TypedResourceFrame& operator=(TypedResourceFrame&&) = default;
        template<std::uint32_t set, std::uint32_t binding, size_t element = 0>
        requires(!std::is_void_v<typename Schema::template Spec<set, binding>> &&
                 !Schema::template Spec<set, binding>::EXTERNAL && element < Schema::template Binding<set, binding>::POINT.amount)
        const auto& buffer() const;
        template<std::uint32_t set, std::uint32_t binding, size_t element = 0>
        requires(!std::is_void_v<typename Schema::template Spec<set, binding>> &&
                 !Schema::template Spec<set, binding>::EXTERNAL && Schema::template Spec<set, binding>::SCOPE == ResourceScope::PER_FRAME &&
                 element < Schema::template Binding<set, binding>::POINT.amount)
        auto& buffer();
        template<std::uint32_t set_number>
        const auto& set() const { return std::get<Schema::SetIndex(set_number)>(_sets); }
        template<typename Binding>
        auto bind_resources(const Binding& binding) const;
    };

    template<typename Schema, typename Policy = DefaultResourceStoragePolicy>
    class TypedSharedResources {
        std::shared_ptr<detail::BundleSharedState<Schema, Policy>> _shared;
        friend class TypedResourceBundle<Schema, Policy>;
        explicit TypedSharedResources(std::shared_ptr<detail::BundleSharedState<Schema, Policy>> shared) : _shared(std::move(shared)) {}

    public:
        template<std::uint32_t set, std::uint32_t binding, size_t element = 0>
        requires(!std::is_void_v<typename Schema::template Spec<set, binding>> &&
                 !Schema::template Spec<set, binding>::EXTERNAL && Schema::template Spec<set, binding>::SCOPE == ResourceScope::SHARED &&
                 element < Schema::template Binding<set, binding>::POINT.amount)
        auto& buffer() { return std::get<Schema::Index(set, binding)>(_shared->resources).value()[element]; }
    };

    template<typename Schema, typename Policy>
    class TypedResourceBundle {
        static_assert(detail::bundle_storage_policy_matches<Schema, Policy>(), "Invalid typed resource storage policy");
        ResourceAllocationPlan<Schema> _plan;
        std::shared_ptr<detail::BundleSharedState<Schema, Policy>> _shared;
        TypedFrameResources<TypedResourceFrame<Schema, Policy>> _frames;
        bool _started = false, _initializing = false, _initialization_failed = false, _shared_initialized = false;
        void require_available() const;

    public:
        using StoragePolicy = Policy;
        using Resources = typename Schema::Resources;
        using Frame = TypedResourceFrame<Schema, Policy>;
        using Shared = TypedSharedResources<Schema, Policy>;
        template<typename... Inputs>
        requires(detail::bundle_inputs_match<Schema, Inputs...>())
        TypedResourceBundle(Policy policy, CommandManager manager, graphic_resource::ManagerInterface& descriptors, const ResourceAllocationSizes<Schema>& sizes, const Inputs&... inputs);
        template<typename... Inputs>
        requires(detail::bundle_inputs_match<Schema, Inputs...>())
        TypedResourceBundle(Policy policy, CommandManager manager, graphic_resource::ManagerInterface& descriptors, const Inputs&... inputs)
            : TypedResourceBundle(std::move(policy), std::move(manager), descriptors, ResourceAllocationSizes<Schema>{}, inputs...) {}
        template<typename... Inputs>
        requires(std::default_initializable<Policy> && detail::bundle_inputs_match<Schema, Inputs...>())
        TypedResourceBundle(CommandManager manager, graphic_resource::ManagerInterface& descriptors, const ResourceAllocationSizes<Schema>& sizes, const Inputs&... inputs)
            : TypedResourceBundle(Policy{}, std::move(manager), descriptors, sizes, inputs...) {}
        template<typename... Inputs>
        requires(std::default_initializable<Policy> && detail::bundle_inputs_match<Schema, Inputs...>())
        TypedResourceBundle(CommandManager manager, graphic_resource::ManagerInterface& descriptors, const Inputs&... inputs)
            : TypedResourceBundle(Policy{}, std::move(manager), descriptors, ResourceAllocationSizes<Schema>{}, inputs...) {}
        TypedResourceBundle(const TypedResourceBundle&) = delete;
        TypedResourceBundle& operator=(const TypedResourceBundle&) = delete;
        TypedResourceBundle(TypedResourceBundle&&) noexcept = default;
        TypedResourceBundle& operator=(TypedResourceBundle&&) noexcept = default;
        const ResourceAllocationPlan<Schema>& plan() const { return _plan; }
        size_t frame_count() const { return _frames.frame_count(); }
        template<typename Initialize>
        requires std::invocable<Initialize&, TypedSharedResources<Schema, Policy>&> && std::same_as<std::invoke_result_t<Initialize&, TypedSharedResources<Schema, Policy>&>, void>
        void initialize_shared(Initialize&& initialize);
        template<typename Update>
        requires std::invocable<Update&, TypedResourceFrame<Schema, Policy>&> && std::same_as<std::invoke_result_t<Update&, TypedResourceFrame<Schema, Policy>&>, void>
        void update(const ComputePhase& phase, Update&& update);
        const Frame& get(const ComputePhase& phase);
    };

    template<typename Schema, typename Policy>
    TypedResourceFrame<Schema, Policy>::TypedResourceFrame(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager,
                                                           std::shared_ptr<detail::BundleSharedState<Schema, Policy>> shared, const ResourceAllocationPlan<Schema>& plan, Policy& policy)
        : _shared(std::move(shared)), _resources(detail::make_bundle_storage<ResourceScope::PER_FRAME>(device, plan, policy, std::tuple<>{})),
          _sets(detail::make_bundle_sets<Schema>(device, manager, _resources, *_shared)) {}

    template<typename Schema, typename Policy>
    template<std::uint32_t set, std::uint32_t binding, size_t element>
    requires(!std::is_void_v<typename Schema::template Spec<set, binding>> &&
             !Schema::template Spec<set, binding>::EXTERNAL && element < Schema::template Binding<set, binding>::POINT.amount)
    const auto& TypedResourceFrame<Schema, Policy>::buffer() const {
        using Spec = typename Schema::template Spec<set, binding>;
        return detail::bundle_values<Schema, Spec>(_resources, *_shared)[element];
    }
    template<typename Schema, typename Policy>
    template<std::uint32_t set, std::uint32_t binding, size_t element>
    requires(!std::is_void_v<typename Schema::template Spec<set, binding>> &&
             !Schema::template Spec<set, binding>::EXTERNAL && Schema::template Spec<set, binding>::SCOPE == ResourceScope::PER_FRAME &&
             element < Schema::template Binding<set, binding>::POINT.amount)
    auto& TypedResourceFrame<Schema, Policy>::buffer() {
        return std::get<Schema::Index(set, binding)>(_resources).value()[element];
    }
    template<typename Schema, typename Policy>
    template<typename Binding>
    auto TypedResourceFrame<Schema, Policy>::bind_resources(const Binding& binding) const {
        return std::apply([&](const auto&... sets) { return binding.bind_resources(sets...); }, _sets);
    }

    template<typename Schema, typename Policy>
    template<typename... Inputs>
    requires(detail::bundle_inputs_match<Schema, Inputs...>())
    TypedResourceBundle<Schema, Policy>::TypedResourceBundle(Policy policy, CommandManager manager, graphic_resource::ManagerInterface& descriptors,
                                                             const ResourceAllocationSizes<Schema>& sizes, const Inputs&... inputs)
        : _plan(detail::bundle_frame_count(manager), sizes),
          _shared(detail::make_bundle_shared<Schema>(manager.get_device(), descriptors, _plan, policy, std::tie(inputs...))),
          _frames(manager, [&](size_t) { return Frame(manager.get_device(), descriptors, _shared, _plan, policy); }) {}

    template<typename Schema, typename Policy>
    void TypedResourceBundle<Schema, Policy>::require_available() const {
        if (!_shared || _initializing || _initialization_failed)
            throw std::logic_error("Typed resource bundle is moved, initializing, or has a failed shared initialization");
    }
    template<typename Schema, typename Policy>
    template<typename Initialize>
    requires std::invocable<Initialize&, TypedSharedResources<Schema, Policy>&> && std::same_as<std::invoke_result_t<Initialize&, TypedSharedResources<Schema, Policy>&>, void>
    void TypedResourceBundle<Schema, Policy>::initialize_shared(Initialize&& initialize) {
        require_available();
        if (_started || _shared_initialized)
            throw std::logic_error("Shared typed resources must be initialized once before frame use");
        _initializing = true;
        try {
            Shared shared(_shared);
            std::invoke(initialize, shared);
        } catch (...) {
            _initializing = false;
            _initialization_failed = true;
            throw;
        }
        _initializing = false;
        _shared_initialized = true;
    }
    template<typename Schema, typename Policy>
    template<typename Update>
    requires std::invocable<Update&, TypedResourceFrame<Schema, Policy>&> && std::same_as<std::invoke_result_t<Update&, TypedResourceFrame<Schema, Policy>&>, void>
    void TypedResourceBundle<Schema, Policy>::update(const ComputePhase& phase, Update&& update) {
        require_available();
        _frames.update(phase, [&](Frame& frame) {
            _started = true;
            std::invoke(update, frame);
        });
    }
    template<typename Schema, typename Policy>
    const TypedResourceFrame<Schema, Policy>& TypedResourceBundle<Schema, Policy>::get(const ComputePhase& phase) {
        require_available();
        const auto& frame = _frames.get(phase);
        _started = true;
        return frame;
    }
} // namespace VSL_NAMESPACE::typed
