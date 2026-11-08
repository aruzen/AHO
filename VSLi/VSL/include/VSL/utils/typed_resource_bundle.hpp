#pragma once
#include "typed_resource_bundle_detail.hpp"

namespace VSL_NAMESPACE::typed {
    template<typename Schema, typename StoragePolicy = HeapStorage>
    class TypedResourceBundle;

    template<typename Schema, typename StoragePolicy = HeapStorage>
    class TypedResourceFrame {
        std::shared_ptr<detail::BundleSharedState<Schema, StoragePolicy>> _shared;
        std::optional<lifetime::ResourceArena> _arena;
        detail::BundleNativeResources _native;
        detail::BundleStorageType<Schema, StoragePolicy> _resources;
        typename detail::BundleSets<typename Schema::Resources::Sets>::Type _sets;
        friend class TypedResourceBundle<Schema, StoragePolicy>;
        TypedResourceFrame(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager,
                           std::shared_ptr<detail::BundleSharedState<Schema, StoragePolicy>> shared, const ResourceAllocationPlan<Schema>& plan, size_t instance);

    public:
        TypedResourceFrame(const TypedResourceFrame&) = delete;
        TypedResourceFrame& operator=(const TypedResourceFrame&) = delete;
        TypedResourceFrame(TypedResourceFrame&&) noexcept = default;
        TypedResourceFrame& operator=(TypedResourceFrame&&) noexcept = default;
        template<std::uint32_t set, std::uint32_t binding, size_t element = 0>
        requires(!std::is_void_v<typename Schema::template Spec<set, binding>> &&
                 !Schema::template Spec<set, binding>::EXTERNAL && detail::buffer_descriptor(Schema::template Binding<set, binding>::POINT.binding_type) && element < Schema::template Binding<set, binding>::POINT.amount)
        const auto& buffer() const;
        template<std::uint32_t set, std::uint32_t binding, size_t element = 0>
        requires(!std::is_void_v<typename Schema::template Spec<set, binding>> &&
                 !Schema::template Spec<set, binding>::EXTERNAL && Schema::template Spec<set, binding>::SCOPE == ResourceScope::PER_FRAME &&
                 detail::buffer_descriptor(Schema::template Binding<set, binding>::POINT.binding_type) && element < Schema::template Binding<set, binding>::POINT.amount)
        auto& buffer();
        template<std::uint32_t set, std::uint32_t binding, size_t element = 0>
        requires(!std::is_void_v<typename Schema::template Spec<set, binding>> && !Schema::template Spec<set, binding>::EXTERNAL && element < Schema::template Binding<set, binding>::POINT.amount)
        const auto& resource() const;
        template<std::uint32_t set, std::uint32_t binding, size_t element = 0>
        requires(Schema::template HasImage<set, binding>() && element < Schema::template Binding<set, binding>::POINT.amount)
        ImageAccessor image() const { return {resource<set, binding, element>().descriptor().image.share()}; }
        template<std::uint32_t set, std::uint32_t binding, size_t element = 0>
        requires(Schema::template HasSampler<set, binding>() && element < Schema::template Binding<set, binding>::POINT.amount)
        SamplerAccessor sampler() const { return {resource<set, binding, element>().descriptor().sampler.share()}; }
        template<std::uint32_t set_number>
        const auto& set() const { return std::get<Schema::SetIndex(set_number)>(_sets); }
        template<typename Binding>
        auto bind_resources(const Binding& binding) const;
    };

    template<typename Schema, typename StoragePolicy = HeapStorage>
    class TypedSharedResources {
        std::shared_ptr<detail::BundleSharedState<Schema, StoragePolicy>> _shared;
        friend class TypedResourceBundle<Schema, StoragePolicy>;
        explicit TypedSharedResources(std::shared_ptr<detail::BundleSharedState<Schema, StoragePolicy>> shared) : _shared(std::move(shared)) {}

    public:
        template<std::uint32_t set, std::uint32_t binding, size_t element = 0>
        requires(!std::is_void_v<typename Schema::template Spec<set, binding>> &&
                 !Schema::template Spec<set, binding>::EXTERNAL && Schema::template Spec<set, binding>::SCOPE == ResourceScope::SHARED &&
                 detail::buffer_descriptor(Schema::template Binding<set, binding>::POINT.binding_type) && element < Schema::template Binding<set, binding>::POINT.amount)
        auto& buffer() { return std::get<Schema::Index(set, binding)>(_shared->resources).value()[element]; }
        template<std::uint32_t set, std::uint32_t binding, size_t element = 0>
        requires(Schema::template HasImage<set, binding>() && Schema::template Spec<set, binding>::IMAGE_SCOPE == ResourceScope::SHARED && element < Schema::template Binding<set, binding>::POINT.amount)
        ImageAccessor image() const;
        template<std::uint32_t set, std::uint32_t binding, size_t element = 0>
        requires(Schema::template HasSampler<set, binding>() && Schema::template Spec<set, binding>::SAMPLER_SCOPE == ResourceScope::SHARED && element < Schema::template Binding<set, binding>::POINT.amount)
        SamplerAccessor sampler() const;
    };

    template<typename Schema, typename StoragePolicy>
    class TypedResourceBundle {
        using PlacementState = std::conditional_t<detail::BundlePolicy<StoragePolicy>::ARENA, std::optional<detail::BundlePlacementPlan<Schema, StoragePolicy>>, std::nullopt_t>;
        ResourceAllocationPlan<Schema> _plan;
        [[no_unique_address]] PlacementState _placement;
        std::shared_ptr<detail::BundleSharedState<Schema, StoragePolicy>> _shared;
        TypedFrameResources<TypedResourceFrame<Schema, StoragePolicy>, detail::BundlePolicy<StoragePolicy>::RETENTION> _frames;
        bool _started = false, _initializing = false, _initialization_failed = false, _shared_initialized = false;
        void require_available() const;

    public:
        using Resources = typename Schema::Resources;
        using Frame = TypedResourceFrame<Schema, StoragePolicy>;
        using Shared = TypedSharedResources<Schema, StoragePolicy>;
        template<typename... Inputs>
        requires(detail::bundle_inputs_match<Schema, Inputs...>())
        TypedResourceBundle(CommandManager manager, graphic_resource::ManagerInterface& descriptors, const ResourceAllocationSizes<Schema>& sizes, const Inputs&... inputs);
        template<typename... Inputs>
        requires(detail::bundle_inputs_match<Schema, Inputs...>())
        TypedResourceBundle(CommandManager manager, graphic_resource::ManagerInterface& descriptors, const Inputs&... inputs)
            : TypedResourceBundle(std::move(manager), descriptors, ResourceAllocationSizes<Schema>{}, inputs...) {}
        TypedResourceBundle(const TypedResourceBundle&) = delete;
        TypedResourceBundle& operator=(const TypedResourceBundle&) = delete;
        TypedResourceBundle(TypedResourceBundle&&) noexcept = default;
        TypedResourceBundle& operator=(TypedResourceBundle&&) noexcept = default;
        const ResourceAllocationPlan<Schema>& plan() const { return _plan; }
        const auto& placement_plan() const&
        requires(detail::BundlePolicy<StoragePolicy>::ARENA)
        { return _placement.value(); }
        const auto& placement_plan() const&&
        requires(detail::BundlePolicy<StoragePolicy>::ARENA)
        = delete;
        size_t frame_count() const { return _frames.frame_count(); }
        template<typename Initialize>
        requires std::invocable<Initialize&, TypedSharedResources<Schema, StoragePolicy>&> && std::same_as<std::invoke_result_t<Initialize&, TypedSharedResources<Schema, StoragePolicy>&>, void>
        void initialize_shared(Initialize&& initialize);
        template<typename Update>
        requires std::invocable<Update&, TypedResourceFrame<Schema, StoragePolicy>&> && std::same_as<std::invoke_result_t<Update&, TypedResourceFrame<Schema, StoragePolicy>&>, void>
        void update(const ComputePhase& phase, Update&& update);
        const Frame& get(const ComputePhase& phase);
    };

    template<typename Schema, typename StoragePolicy>
    TypedResourceFrame<Schema, StoragePolicy>::TypedResourceFrame(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager,
                                                                  std::shared_ptr<detail::BundleSharedState<Schema, StoragePolicy>> shared, const ResourceAllocationPlan<Schema>& plan, size_t instance)
        : _shared(std::move(shared)), _arena(detail::make_bundle_arena<ResourceScope::PER_FRAME, Schema, StoragePolicy>(device, plan, instance)),
          _native(detail::make_bundle_native<ResourceScope::PER_FRAME, Schema, StoragePolicy>(device, plan)),
          _resources(detail::make_bundle_storage<ResourceScope::PER_FRAME, Schema, StoragePolicy>(device, plan, std::tuple<>{}, _arena ? &*_arena : nullptr, _native, *_shared)),
          _sets(detail::make_bundle_sets<Schema, StoragePolicy>(device, manager, _resources, *_shared)) {}

    template<typename Schema, typename StoragePolicy>
    template<std::uint32_t set, std::uint32_t binding, size_t element>
    requires(!std::is_void_v<typename Schema::template Spec<set, binding>> &&
             !Schema::template Spec<set, binding>::EXTERNAL && detail::buffer_descriptor(Schema::template Binding<set, binding>::POINT.binding_type) && element < Schema::template Binding<set, binding>::POINT.amount)
    const auto& TypedResourceFrame<Schema, StoragePolicy>::buffer() const {
        return resource<set, binding, element>();
    }
    template<typename Schema, typename StoragePolicy>
    template<std::uint32_t set, std::uint32_t binding, size_t element>
    requires(!std::is_void_v<typename Schema::template Spec<set, binding>> && !Schema::template Spec<set, binding>::EXTERNAL && element < Schema::template Binding<set, binding>::POINT.amount)
    const auto& TypedResourceFrame<Schema, StoragePolicy>::resource() const {
        using Spec = typename Schema::template Spec<set, binding>;
        return detail::bundle_values<Schema, Spec, StoragePolicy>(_resources, *_shared)[element];
    }
    template<typename Schema, typename StoragePolicy>
    template<std::uint32_t set, std::uint32_t binding, size_t element>
    requires(!std::is_void_v<typename Schema::template Spec<set, binding>> &&
             !Schema::template Spec<set, binding>::EXTERNAL && Schema::template Spec<set, binding>::SCOPE == ResourceScope::PER_FRAME &&
             detail::buffer_descriptor(Schema::template Binding<set, binding>::POINT.binding_type) && element < Schema::template Binding<set, binding>::POINT.amount)
    auto& TypedResourceFrame<Schema, StoragePolicy>::buffer() {
        return std::get<Schema::Index(set, binding)>(_resources).value()[element];
    }
    template<typename Schema, typename StoragePolicy>
    template<std::uint32_t set, std::uint32_t binding, size_t element>
    requires(Schema::template HasImage<set, binding>() && Schema::template Spec<set, binding>::IMAGE_SCOPE == ResourceScope::SHARED && element < Schema::template Binding<set, binding>::POINT.amount)
    ImageAccessor TypedSharedResources<Schema, StoragePolicy>::image() const {
        constexpr auto component = Schema::template Spec<set, binding>::ALLOCATION == AllocationKind::TEXTURE ? ResourceComponent::IMAGE : ResourceComponent::PRIMARY;
        return detail::bundle_image(*_shared, nullptr, _shared->native, _shared->layout.placement({set, binding, element}, component));
    }
    template<typename Schema, typename StoragePolicy>
    template<std::uint32_t set, std::uint32_t binding, size_t element>
    requires(Schema::template HasSampler<set, binding>() && Schema::template Spec<set, binding>::SAMPLER_SCOPE == ResourceScope::SHARED && element < Schema::template Binding<set, binding>::POINT.amount)
    SamplerAccessor TypedSharedResources<Schema, StoragePolicy>::sampler() const {
        constexpr auto component = Schema::template Spec<set, binding>::ALLOCATION == AllocationKind::TEXTURE ? ResourceComponent::SAMPLER : ResourceComponent::PRIMARY;
        return detail::bundle_sampler(*_shared, nullptr, _shared->native, _shared->layout.placement({set, binding, element}, component));
    }
    template<typename Schema, typename StoragePolicy>
    template<typename Binding>
    auto TypedResourceFrame<Schema, StoragePolicy>::bind_resources(const Binding& binding) const {
        return std::apply([&](const auto&... sets) { return binding.bind_resources(sets...); }, _sets);
    }

    template<typename Schema, typename StoragePolicy>
    template<typename... Inputs>
    requires(detail::bundle_inputs_match<Schema, Inputs...>())
    TypedResourceBundle<Schema, StoragePolicy>::TypedResourceBundle(CommandManager manager, graphic_resource::ManagerInterface& descriptors,
                                                                    const ResourceAllocationSizes<Schema>& sizes, const Inputs&... inputs)
        : _plan(detail::bundle_frame_count(manager), sizes),
          _placement([&]() -> PlacementState {
              if constexpr (detail::BundlePolicy<StoragePolicy>::ARENA)
                  return detail::BundlePlacementPlan<Schema, StoragePolicy>(_plan);
              return std::nullopt;
          }()),
          _shared(detail::make_bundle_shared<Schema, StoragePolicy>(manager.get_device(), descriptors, _plan, std::tie(inputs...))),
          _frames(manager, [&](size_t instance) { return Frame(manager.get_device(), descriptors, _shared, _plan, instance); }) {}

    template<typename Schema, typename StoragePolicy>
    void TypedResourceBundle<Schema, StoragePolicy>::require_available() const {
        if (!_shared || _initializing || _initialization_failed)
            throw std::logic_error("Typed resource bundle is moved, initializing, or has a failed shared initialization");
    }
    template<typename Schema, typename StoragePolicy>
    template<typename Initialize>
    requires std::invocable<Initialize&, TypedSharedResources<Schema, StoragePolicy>&> && std::same_as<std::invoke_result_t<Initialize&, TypedSharedResources<Schema, StoragePolicy>&>, void>
    void TypedResourceBundle<Schema, StoragePolicy>::initialize_shared(Initialize&& initialize) {
        require_available();
        if (_started || _shared_initialized)
            throw std::logic_error("Shared typed resources must be initialized once before frame use");
        _initializing = true;
        try {
            Shared shared(_shared);
            std::invoke(initialize, shared);
            detail::require_bundle_shared_idle(*_shared);
        } catch (...) {
            _initializing = false;
            _initialization_failed = true;
            throw;
        }
        _initializing = false;
        _shared_initialized = true;
    }
    template<typename Schema, typename StoragePolicy>
    template<typename Update>
    requires std::invocable<Update&, TypedResourceFrame<Schema, StoragePolicy>&> && std::same_as<std::invoke_result_t<Update&, TypedResourceFrame<Schema, StoragePolicy>&>, void>
    void TypedResourceBundle<Schema, StoragePolicy>::update(const ComputePhase& phase, Update&& update) {
        require_available();
        _frames.update(phase, [&](Frame& frame) {
            _started = true;
            std::invoke(update, frame);
        });
    }
    template<typename Schema, typename StoragePolicy>
    const TypedResourceFrame<Schema, StoragePolicy>& TypedResourceBundle<Schema, StoragePolicy>::get(const ComputePhase& phase) {
        require_available();
        const auto& frame = _frames.get(phase);
        _started = true;
        return frame;
    }
} // namespace VSL_NAMESPACE::typed
