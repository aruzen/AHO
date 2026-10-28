#ifdef AHO_GPU_TESTS
#include <AHO/resource/typed_storage_policy.hpp>
#include "support/typed_storage_fixture.hpp"

namespace {
    namespace t = vsl::typed;
    namespace r = aho::resource;
    namespace s = vsl_test::storage_policy;
    class TypedAHOStorageVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }

    protected:
        template<typename Policy>
        void verify_slots(Policy policy);
    };
    struct Customization {
        std::shared_ptr<s::Counters> counters;
        std::shared_ptr<int> owner;
        template<typename Native>
        auto map(Native& buffer) {
            ++counters->maps;
            return buffer.data();
        }
    };
    struct CustomFactory {
        std::shared_ptr<s::Counters> counters;
        std::unique_ptr<int> state = std::make_unique<int>(7);
        explicit CustomFactory(std::shared_ptr<s::Counters> counters) : counters(std::move(counters)) {}
        template<typename Binding, typename Allocation>
        auto make_buffer(vsl::LogicalDeviceAccessor device, size_t bytes, size_t element);
    };
    using CustomPolicy = r::TypedStoragePolicy<CustomFactory>;

    template<typename Binding, typename Allocation>
    auto CustomFactory::make_buffer(vsl::LogicalDeviceAccessor device, size_t bytes, size_t element) {
        if (!state || counters->calls.size() == counters->fail_after)
            throw std::runtime_error("Injected custom factory failure");
        constexpr auto usage = Binding::POINT.binding_type == vsl::graphic_resource::Type::UNIFORM_BUFFER ? vsl::MemoryType::UNIFORM_BUFFER : vsl::MemoryType::STORAGE_BUFFER;
        using Native = vsl::Buffer<usage, vsl::MemoryProperty::HOST_VISIBLE>;
        auto owner = std::make_shared<int>(7);
        counters->calls.push_back({Allocation::SET, Allocation::BINDING, element, bytes});
        counters->owners.push_back(owner);
        return r::make_custom_buffer(Native(device, bytes), Customization{counters, std::move(owner)});
    }

    template<typename Policy>
    void TypedAHOStorageVulkanIntegrationTest::verify_slots(Policy policy) {
        using Bundle = t::TypedResourceBundle<s::Allocation, Policy>;
        auto manager = make_manager().independent(3);
        vsl::GraphicResourceManager descriptors(device);
        Bundle bundle(std::move(policy), manager, descriptors, s::sizes());
        EXPECT_EQ(bundle.plan().totals().buffers, 9u);
        s::initialize_shared(bundle);
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_bundle.comp.spv"));
        t::TypedComputePipeline<s::Resources> pipeline(device, vsl::pipeline_layout::ShaderGroup("AHO policy", {shader}));
        std::array<std::uint32_t, 3> expected{};
        for (std::uint32_t iteration = 0; iteration < 12; ++iteration) {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            const auto slot = phase.frame_slot;
            const auto value = iteration + 100;
            bundle.update(phase, [&](typename Bundle::Frame& frame) {
                const auto bytes = frame.template buffer<2, 3>().backend().read(16);
                std::array<std::uint32_t, 4> previous{};
                std::memcpy(previous.data(), bytes.data(), bytes.size());
                for (size_t index = 0; index < 4; ++index)
                    EXPECT_EQ(previous[index], expected[slot] ? expected[slot] + index : 0);
                frame.template buffer<2, 0>().copy(std::span(&value, 1));
                expected[slot] = value + 60;
            });
            const auto& frame = bundle.get(phase);
            auto alias = frame.template buffer<2, 0>();
            phase << frame.bind_resources(pipeline).dispatch(4);
            EXPECT_TRUE(alias.backend().busy());
            EXPECT_THROW(alias.copy(std::span(&value, 1)), std::logic_error);
            EXPECT_THROW((frame.template buffer<2, 3>().backend().read(16)), std::logic_error);
            s::host_read_barrier(manager, slot);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            manager.get_buffer().next();
        }
        for (size_t slot = 0; slot < 3; ++slot) {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            const auto bytes = bundle.get(phase).template buffer<2, 3>().backend().read(16);
            std::array<std::uint32_t, 4> values{};
            std::memcpy(values.data(), bytes.data(), bytes.size());
            for (size_t index = 0; index < 4; ++index)
                EXPECT_EQ(values[index], expected[slot] + index);
            phase.cancel();
            manager.get_buffer().next();
        }
    }

    TEST_F(TypedAHOStorageVulkanIntegrationTest, HostBuffersPreserveTypedUpdatesAcrossThreeSlotsAndTwelveSubmissions) {
        verify_slots(r::TypedStoragePolicy<>{});
    }

    TEST_F(TypedAHOStorageVulkanIntegrationTest, CustomFactoriesKeepTheirTypesMappingAndDescriptorArrayPositions) {
        auto counters = std::make_shared<s::Counters>();
        verify_slots(CustomPolicy{CustomFactory(counters)});
        ASSERT_EQ(counters->calls.size(), 9u);
        EXPECT_EQ(counters->maps, 9u);
        EXPECT_EQ(std::ranges::count_if(counters->calls, [](const auto& call) { return call.set == 5 && call.element == 1; }), 1);
        EXPECT_EQ(std::ranges::count_if(counters->calls, [](const auto& call) { return call.set == 2 && call.binding == 3 && call.count == 16; }), 3);
        for (const auto& owner : counters->owners)
            EXPECT_TRUE(owner.expired());
    }

    TEST_F(TypedAHOStorageVulkanIntegrationTest, OwnedBuffersZeroChunkBoundariesAndRejectCountOrStorageReplacement) {
        using Host = r::Buffer<r::BufferType::GPU_FRIENDLY>;
        using Buffer = r::TypedResourceBuffer<std::uint32_t, vsl::graphic_resource::Type::STORAGE_BUFFER, Host>;
        std::array<std::uint32_t, 1025> values;
        values.fill(99);
        Host source(device, sizeof(values));
        source.write(std::as_bytes(std::span(values)));
        Buffer buffer(source, std::in_place, values.size());
        EXPECT_TRUE(std::ranges::all_of(buffer.backend().read(sizeof(values)), [](auto byte) { return byte == std::byte{}; }));
        buffer.copy(values);
        EXPECT_EQ(buffer.backend().read(sizeof(values))[0], std::byte{99});
        const auto view = buffer.view(1, 2);
        EXPECT_EQ(view.byte_offset(), 4u);
        EXPECT_EQ(view.size(), 2u);
        EXPECT_EQ(view.descriptor().use.state, buffer.descriptor().use.state);
        EXPECT_THROW(buffer.copy(std::span(values).first(1)), std::invalid_argument);
        buffer.backend().reserve(sizeof(values) * 2);
        EXPECT_THROW(buffer.copy(values), std::logic_error);
    }

    TEST_F(TypedAHOStorageVulkanIntegrationTest, CustomOwnersSurviveBundleDestructionUntilTheSubmissionIsReleased) {
        using Bundle = t::TypedResourceBundle<s::Allocation, CustomPolicy>;
        auto manager = make_manager().independent(1);
        vsl::GraphicResourceManager descriptors(device);
        auto counters = std::make_shared<s::Counters>();
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_bundle.comp.spv"));
        t::TypedComputePipeline<s::Resources> pipeline(device, vsl::pipeline_layout::ShaderGroup("policy lifetime", {shader}));
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            {
                Bundle bundle(CustomPolicy{CustomFactory(counters)}, manager, descriptors, s::sizes());
                s::initialize_shared(bundle);
                phase << bundle.get(phase).bind_resources(pipeline).dispatch(4);
            }
            ASSERT_EQ(counters->owners.size(), 5u);
            for (const auto& owner : counters->owners)
                EXPECT_FALSE(owner.expired());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        auto phase = manager.start_phase<vsl::ComputePhase>();
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        for (const auto& owner : counters->owners)
            EXPECT_TRUE(owner.expired());
    }

    TEST_F(TypedAHOStorageVulkanIntegrationTest, FailedCustomFactoryReleasesEveryConstructedOwner) {
        using Bundle = t::TypedResourceBundle<s::Allocation, CustomPolicy>;
        auto counters = std::make_shared<s::Counters>();
        counters->fail_after = 6;
        auto manager = make_manager().independent(3);
        vsl::GraphicResourceManager descriptors(device);
        EXPECT_THROW((Bundle(CustomPolicy{CustomFactory(counters)}, manager, descriptors, s::sizes())), std::runtime_error);
        ASSERT_EQ(counters->owners.size(), 6u);
        for (const auto& owner : counters->owners)
            EXPECT_TRUE(owner.expired());
    }
} // namespace
#endif
