#include <VSL/utils/typed_frame_resources.hpp>
#include <VSL/utils/typed_compute.hpp>
#include "support/vulkan_fixture.hpp"

namespace {
    namespace t = vsl::typed;
    struct FrameValue {
        size_t index;
        int value = 0;
    };
    class TypedFrameVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };

    TEST_F(TypedFrameVulkanIntegrationTest, FactoryCreatesOneRecordPerCommandSlotAndReusesItsIdentity) {
        auto manager = make_manager().independent(3);
        std::vector<size_t> created;
        t::TypedFrameResources<FrameValue> frames(manager, [&](size_t index) {
            created.push_back(index);
            return FrameValue{index};
        });
        EXPECT_EQ(created, (std::vector<size_t>{0, 1, 2}));
        EXPECT_EQ(frames.frame_count(), 3u);
        std::array<const FrameValue*, 3> records{};
        for (size_t iteration = 0; iteration < 9; ++iteration) {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            frames.update(phase, [&](FrameValue& value) {
                EXPECT_EQ(value.index, iteration % 3);
                EXPECT_EQ(value.value, int(iteration / 3));
                ++value.value;
            });
            const auto& value = frames.get(phase);
            if (iteration < 3)
                records[iteration] = &value;
            EXPECT_EQ(&value, records[iteration % 3]);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            manager.get_buffer().next();
        }
        EXPECT_EQ(created.size(), 3u);
    }

    TEST_F(TypedFrameVulkanIntegrationTest, UpdatesAreOncePerPhaseAndGetFreezesInitialValues) {
        auto manager = make_manager();
        t::TypedFrameResources<FrameValue> frames(manager, [](size_t index) { return FrameValue{index}; });
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            frames.update(phase, [](FrameValue& value) { value.value = 42; });
            EXPECT_EQ(frames.get(phase).value, 42);
            EXPECT_EQ(&frames.get(phase), &frames.get(phase));
            EXPECT_THROW(frames.update(phase, [](FrameValue&) {}), std::logic_error);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            EXPECT_THROW(frames.get(phase), std::logic_error);
        }
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            EXPECT_EQ(frames.get(phase).value, 42);
            EXPECT_THROW(frames.update(phase, [](FrameValue&) {}), std::logic_error);
        }
    }

    TEST_F(TypedFrameVulkanIntegrationTest, RejectsForeignManagersUnpreparedCancelledAndForgedRecordingState) {
        auto manager = make_manager();
        auto other = manager.independent();
        t::TypedFrameResources<FrameValue> frames(manager, [](size_t index) { return FrameValue{index}; });
        {
            auto phase = other.start_phase<vsl::ComputePhase>();
            EXPECT_THROW(frames.get(phase), std::invalid_argument);
        }
        auto phase = manager.start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, std::nullopt, true);
        EXPECT_THROW(frames.get(phase), std::logic_error);
        phase.state = vsl::ComputePhase::State::WAIT_SUBMIT;
        EXPECT_THROW(frames.get(phase), std::logic_error);
        phase.state = vsl::ComputePhase::State::BEFORE_SETUP;
        phase.setup();
        const auto slot = phase.frame_slot;
        phase.frame_slot = UINT32_MAX;
        EXPECT_THROW(frames.get(phase), std::logic_error);
        phase.frame_slot = slot;
        EXPECT_NO_THROW(frames.get(phase));
        phase.cancel();
        EXPECT_THROW(frames.get(phase), std::logic_error);
        phase.state = vsl::ComputePhase::State::WAIT_SUBMIT;
        EXPECT_THROW(frames.get(phase), std::logic_error);
        phase.state = vsl::ComputePhase::State::ERROR_;
    }

    TEST_F(TypedFrameVulkanIntegrationTest, RejectsReplacedCommandBufferRingsAndEmptyManagers) {
        EXPECT_THROW((t::TypedFrameResources<FrameValue>(vsl::CommandManager(nullptr), [](size_t index) { return FrameValue{index}; })), std::invalid_argument);
        auto manager = make_manager();
        const auto original = manager.get_buffer();
        t::TypedFrameResources<FrameValue> frames(manager, [](size_t index) { return FrameValue{index}; });
        manager.data_->command_buffer = manager.make_exclusive_buffer().data_;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            EXPECT_THROW(frames.get(phase), std::invalid_argument);
            phase.cancel();
        }
        manager.data_->command_buffer = original.data_;
        auto phase = manager.start_phase<vsl::ComputePhase>();
        EXPECT_NO_THROW(frames.get(phase));
    }

    TEST_F(TypedFrameVulkanIntegrationTest, FailedAndReentrantUpdatesAreRejectedUntilTheNextPhase) {
        auto manager = make_manager();
        t::TypedFrameResources<FrameValue> frames(manager, [](size_t index) { return FrameValue{index}; });
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            EXPECT_THROW(frames.update(phase, [&](FrameValue&) {
                EXPECT_THROW(frames.get(phase), std::logic_error);
                EXPECT_THROW(frames.update(phase, [](FrameValue&) {}), std::logic_error);
                throw std::runtime_error("Update failed");
            }),
                         std::runtime_error);
            EXPECT_THROW(frames.get(phase), std::logic_error);
            EXPECT_THROW(frames.update(phase, [](FrameValue&) {}), std::logic_error);
            phase.cancel();
        }
        auto phase = manager.start_phase<vsl::ComputePhase>();
        frames.update(phase, [](FrameValue& value) { value.value = 7; });
        EXPECT_EQ(frames.get(phase).value, 7);
    }

    TEST_F(TypedFrameVulkanIntegrationTest, PhaseRetainsRecordsAfterTheContainerIsDestroyedOrMoved) {
        struct Owners {
            std::shared_ptr<int> owner;
        };
        auto manager = make_manager().independent(1);
        std::weak_ptr<int> owner;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            {
                t::TypedFrameResources<Owners> original(manager, [&](size_t) {
                    auto value = std::make_shared<int>(9);
                    owner = value;
                    return Owners{std::move(value)};
                });
                auto frames = std::move(original);
                EXPECT_THROW(original.get(phase), std::invalid_argument);
                EXPECT_EQ(*frames.get(phase).owner, 9);
            }
            ASSERT_FALSE(owner.expired());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        EXPECT_FALSE(owner.expired());
        auto phase = manager.start_phase<vsl::ComputePhase>();
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        EXPECT_TRUE(owner.expired());
    }

    using FrameSet = t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>,
                                      t::StorageBinding<1, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
    struct ComputeResources {
        t::TypedUniformBuffer<std::uint32_t> uniform;
        t::TypedStorageBuffer<std::uint32_t> output;
        t::TypedDescriptorSet<FrameSet> set;
        std::uint32_t expected = 0;
        ComputeResources(vsl::LogicalDeviceAccessor device, vsl::graphic_resource::ManagerInterface& manager);
    };
    ComputeResources::ComputeResources(vsl::LogicalDeviceAccessor device, vsl::graphic_resource::ManagerInterface& manager)
        : uniform(device, std::uint32_t{0}), output(device, std::uint32_t{0}), set(device, manager) {
        set.write<0>(uniform);
        set.write<1>(output);
    }

    TEST_F(TypedFrameVulkanIntegrationTest, PerSlotUniformsAndDescriptorsReachTheGpuAcrossRepeatedReuse) {
        auto manager = make_manager().independent(3);
        vsl::GraphicResourceManager descriptors(device);
        t::TypedFrameResources<ComputeResources> frames(manager, [&](size_t) { return ComputeResources(device, descriptors); });
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_frame.comp.spv"));
        t::TypedComputePipeline<t::ResourceSchema<FrameSet>> pipeline(device, vsl::pipeline_layout::ShaderGroup("typed frames", {shader}));
        std::array<vsl::Completion, 3> previous;
        std::array<std::shared_ptr<vsl::impl::GraphicResourceImpl>, 3> sets;
        for (std::uint32_t iteration = 0; iteration < 12; ++iteration) {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            const auto slot = phase.frame_slot;
            if (previous[slot])
                EXPECT_EQ(previous[slot].status(), vsl::Completion::Status::COMPLETE);
            frames.update(phase, [&](ComputeResources& resources) {
                auto mapped = resources.output.backend().data();
                EXPECT_EQ(*static_cast<const std::uint32_t*>(mapped.data), resources.expected);
                resources.expected = iteration + 100;
                resources.uniform.copy(std::span(&resources.expected, 1));
            });
            const auto& current = frames.get(phase);
            if (iteration < 3)
                sets[slot] = current.set.backend().data_;
            EXPECT_EQ(current.set.backend().data_, sets[slot]);
            phase << pipeline.bind_resources(current.set).dispatch(1);
            auto buffer = manager.get_buffer();
            vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
            buffer.data_->command_buffers.at(slot).pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
            previous[slot] = phase.completion();
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            manager.get_buffer().next();
        }
        EXPECT_NE(sets[0], sets[1]);
        EXPECT_NE(sets[1], sets[2]);
        EXPECT_NE(sets[0], sets[2]);
        for (size_t index = 0; index < frames.frame_count(); ++index) {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            const auto& current = frames.get(phase);
            auto buffer = current.output.backend();
            auto mapped = buffer.data();
            EXPECT_EQ(*static_cast<const std::uint32_t*>(mapped.data), current.expected);
            phase.cancel();
            manager.get_buffer().next();
        }
    }
} // namespace
