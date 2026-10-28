#include "support/typed_storage_fixture.hpp"

namespace {
    namespace t = vsl::typed;
    namespace s = vsl_test::storage_policy;
    class TypedStorageVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    struct CountingPolicy : t::DefaultResourceStoragePolicy {
        template<typename Binding, typename Allocation>
        struct Buffer : t::DefaultResourceStoragePolicy::Buffer<Binding, Allocation> {
            using Base = t::DefaultResourceStoragePolicy::Buffer<Binding, Allocation>;
            explicit Buffer(Base value) : Base(std::move(value)) {}
            Buffer(Buffer&&) = default;
            Buffer& operator=(Buffer&&) = delete;
        };
        std::shared_ptr<s::Counters> counters;
        std::unique_ptr<int> state = std::make_unique<int>(7);
        explicit CountingPolicy(std::shared_ptr<s::Counters> counters) : counters(std::move(counters)) {}
        template<typename Binding, typename Allocation>
        Buffer<Binding, Allocation> make_buffer(vsl::LogicalDeviceAccessor device, size_t count, size_t element);
    };
    using Bundle = t::TypedResourceBundle<s::Allocation, CountingPolicy>;
    struct InvalidPolicy {
        enum class Mode {
            COUNT,
            RANGE,
            TRACKING
        } mode;
        template<typename Binding, typename Allocation>
        struct Buffer {
            using Payload = typename Binding::Payload;
            static constexpr auto KIND = Binding::POINT.binding_type;
            t::TypedResourceBuffer<Payload, KIND> value;
            Mode mode;
            size_t size() const { return value.size() + (mode == Mode::COUNT); }
            void copy(std::span<const Payload> values) { value.copy(values); }
            t::detail::DescriptorValue descriptor() const;
        };
        template<typename Binding, typename Allocation>
        Buffer<Binding, Allocation> make_buffer(vsl::LogicalDeviceAccessor device, size_t count, size_t) {
            return {t::TypedResourceBuffer<typename Binding::Payload, Binding::POINT.binding_type>(device, std::in_place, count), mode};
        }
    };

    template<typename Binding, typename Allocation>
    auto CountingPolicy::make_buffer(vsl::LogicalDeviceAccessor device, size_t count, size_t element) -> Buffer<Binding, Allocation> {
        if (!state || counters->calls.size() == counters->fail_after)
            throw std::runtime_error("Injected policy failure");
        auto value = t::DefaultResourceStoragePolicy::make_buffer<Binding, Allocation>(device, count, element);
        counters->calls.push_back({Allocation::SET, Allocation::BINDING, element, count});
        counters->owners.push_back(value.descriptor().use.owner);
        return Buffer<Binding, Allocation>(std::move(value));
    }

    TEST_F(TypedStorageVulkanIntegrationTest, StatefulMoveOnlyPolicyPreservesScopesArraysAndTypedUpdates) {
        auto manager = make_manager().independent(3);
        vsl::GraphicResourceManager descriptors(device);
        auto counters = std::make_shared<s::Counters>();
        Bundle bundle(CountingPolicy(counters), manager, descriptors, s::sizes());
        ASSERT_EQ(counters->calls.size(), 9u);
        EXPECT_EQ(bundle.plan().totals().buffers, 9u);
        EXPECT_EQ(std::ranges::count_if(counters->calls, [](const auto& call) { return call.set == 2 && call.binding == 0; }), 3);
        EXPECT_EQ(std::ranges::count_if(counters->calls, [](const auto& call) { return call.set == 5 && call.element == 1; }), 1);
        s::initialize_shared(bundle);
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_bundle.comp.spv"));
        t::TypedComputePipeline<s::Resources> pipeline(device, vsl::pipeline_layout::ShaderGroup("policy", {shader}));
        for (std::uint32_t iteration = 0; iteration < 12; ++iteration) {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            const auto value = iteration + 100;
            bundle.update(phase, [&](Bundle::Frame& frame) { frame.buffer<2, 0>().copy(std::span(&value, 1)); });
            const auto& frame = bundle.get(phase);
            phase << frame.bind_resources(pipeline).dispatch(4);
            s::host_read_barrier(manager, phase.frame_slot);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
            auto native = frame.buffer<2, 3>().backend();
            auto mapped = native.data();
            for (size_t index = 0; index < 4; ++index)
                EXPECT_EQ(static_cast<const std::uint32_t*>(mapped.data)[index], value + 60 + index);
            manager.get_buffer().next();
        }
    }

    TEST_F(TypedStorageVulkanIntegrationTest, FactoryFailureReleasesSharedAndPreviouslyConstructedFrameBuffers) {
        auto manager = make_manager().independent(3);
        vsl::GraphicResourceManager descriptors(device);
        auto counters = std::make_shared<s::Counters>();
        counters->fail_after = 6;
        EXPECT_THROW((Bundle(CountingPolicy(counters), manager, descriptors, s::sizes())), std::runtime_error);
        ASSERT_EQ(counters->owners.size(), 6u);
        for (const auto& owner : counters->owners)
            EXPECT_TRUE(owner.expired());
    }

    template<typename Binding, typename Allocation>
    auto InvalidPolicy::Buffer<Binding, Allocation>::descriptor() const -> t::detail::DescriptorValue {
        auto descriptor = value.descriptor();
        if (mode == Mode::RANGE)
            descriptor.bytes = 0;
        if (mode == Mode::TRACKING)
            descriptor.use = {};
        return descriptor;
    }
    TEST_F(TypedStorageVulkanIntegrationTest, FactoriesMustPreserveCountRangeAndTracking) {
        auto manager = make_manager();
        vsl::GraphicResourceManager descriptors(device);
        for (const auto mode : {InvalidPolicy::Mode::COUNT, InvalidPolicy::Mode::RANGE, InvalidPolicy::Mode::TRACKING})
            EXPECT_THROW((t::TypedResourceBundle<s::Allocation, InvalidPolicy>(InvalidPolicy{mode}, manager, descriptors, s::sizes())), std::invalid_argument);
    }

    TEST_F(TypedStorageVulkanIntegrationTest, ExternalOnlyAllocationDoesNotRequireBufferFactories) {
        struct ExternalOnlyPolicy {};
        using Set = t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
        using Schema = t::ResourceAllocationSchema<t::ResourceSchema<Set>, t::ExternalAllocation<0, 0>>;
        auto manager = make_manager();
        vsl::GraphicResourceManager descriptors(device);
        const std::uint32_t value = 7;
        t::TypedUniformBuffer<std::uint32_t> input(device, value);
        t::TypedResourceBundle<Schema, ExternalOnlyPolicy> bundle(manager, descriptors, {}, t::external_resource<0, 0>(input));
        auto phase = manager.start_phase<vsl::ComputePhase>();
        EXPECT_NO_THROW(bundle.get(phase).set<0>().require_complete());
        EXPECT_EQ(bundle.plan().totals().buffers, 0u);
        phase.cancel();
    }
} // namespace
