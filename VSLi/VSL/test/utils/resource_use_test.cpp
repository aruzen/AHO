#include <VSL/lifetime/buffer_access.hpp>
#include <VSL/lifetime/command_access.hpp>
#include <VSL/vulkan/resource_use.hpp>
#include <VSL/utils/typed_batch.hpp>
#include "support/render_fixture.hpp"

namespace {
    using VSL_NAMESPACE::lifetime::detail::BufferAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace {
    namespace t = vsl::typed;
    class ResourceUseVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    class TrackedDrawVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    struct Vertex {
        float xy[2];
        float rgb[3];
    };
    using Record = t::VertexRecord<Vertex, VSL_VERTEX_MEMBER(Vertex, xy), VSL_VERTEX_MEMBER(Vertex, rgb)>;
    using Input = t::TypedVertexInput<Record::Binding<0>>;
    const std::array<Vertex, 3> VERTICES{{{{-1, -1}, {.25f, .5f, .75f}}, {{3, -1}, {.25f, .5f, .75f}}, {{-1, 3}, {.25f, .5f, .75f}}}};
    using Set = t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>, t::StorageBinding<1, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
    using Schema = t::ResourceSchema<Set>;

    TEST_F(ResourceUseVulkanIntegrationTest, MultipleRecordingPhasesBlockCpuWritesUntilEveryUseFinishes) {
        auto manager = make_manager().independent(1);
        auto other = manager.independent(1);
        vsl::ResourceUseState state;
        EXPECT_FALSE(state.busy());
        EXPECT_TRUE(state.wait(0));
        EXPECT_THROW(state.mark_used({}), std::logic_error);
        auto first = manager.start_phase<vsl::ComputePhase>();
        auto second = other.start_phase<vsl::ComputePhase>();
        state.mark_used(first.completion());
        state.mark_used(first.completion());
        state.mark_used(second.completion());
        EXPECT_TRUE(state.busy());
        EXPECT_FALSE(state.wait(0));
        EXPECT_THROW(state.require_idle(), std::logic_error);
        EXPECT_THROW(state.check_available(first.completion()), std::logic_error);
        ASSERT_EQ(second.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(second.completion().wait());
        EXPECT_TRUE(state.busy());
        first.cancel();
        EXPECT_TRUE(state.wait(0));
        EXPECT_FALSE(state.busy());
    }

    TEST_F(ResourceUseVulkanIntegrationTest, SubmissionRetainsTheOwnerWithoutRetainingTheUseState) {
        auto manager = make_manager().independent(1);
        std::weak_ptr<int> owner;
        std::weak_ptr<vsl::ResourceUseState> state;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            {
                auto value = std::make_shared<int>(9);
                auto tracking = std::make_shared<vsl::ResourceUseState>();
                owner = value;
                state = tracking;
                vsl::ResourceUse{tracking, value}.record(manager.get_buffer());
            }
            EXPECT_TRUE(state.expired());
            EXPECT_FALSE(owner.expired());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        auto phase = manager.start_phase<vsl::ComputePhase>();
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        EXPECT_TRUE(owner.expired());
    }
    TEST_F(ResourceUseVulkanIntegrationTest, SubmitValidationKeepsAnIndependentUncertainStateAliveUntilCompletion) {
        auto commands = make_manager().independent(1);
        auto phase = commands.start_phase<vsl::ComputePhase>();
        std::weak_ptr<vsl::ResourceUseState> observed;
        {
            auto state = std::make_shared<vsl::ResourceUseState>(vsl::UncertainUsePolicy::REQUIRE_RECREATION);
            observed = state;
            vsl::ResourceUse{state, {}}.record(commands.get_buffer(), vsl::ResourceRetention::COMPLETION);
        }
        EXPECT_FALSE(observed.expired());
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        EXPECT_TRUE(observed.expired());
    }

    TEST_F(ResourceUseVulkanIntegrationTest, RetentionUsesControlIdentityIncludingAliasesAndNullPointers) {
        auto manager = make_manager();
        vsl::Completion cancelled;
        std::weak_ptr<std::array<int, 2>> observed;
        size_t releases = 0;
        int marker = 0;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            cancelled = phase.completion();
            {
                auto root = std::make_shared<std::array<int, 2>>();
                observed = root;
                std::shared_ptr<void> first(root, &(*root)[0]), second(root, &(*root)[1]), null_alias(root, nullptr);
                cancelled.retain(first);
                manager.get_buffer().retain(second);
                cancelled.retain(null_alias);
                std::shared_ptr<void> different_a(&marker, [&](void*) { ++releases; });
                std::shared_ptr<void> different_b(&marker, [&](void*) { ++releases; });
                cancelled.retain(std::move(different_a));
                manager.get_buffer().retain(std::move(different_b));
                EXPECT_FALSE(different_a);
                EXPECT_FALSE(different_b);
                EXPECT_EQ(cancelled.data_->work.lock()->resources.size(), 3u);
            }
            EXPECT_EQ(releases, 0u);
            EXPECT_FALSE(observed.expired());
            phase.cancel();
        }
        EXPECT_EQ(cancelled.status(), vsl::Completion::Status::CANCELLED);
        EXPECT_TRUE(observed.expired());
        EXPECT_EQ(releases, 2u);
    }

    TEST_F(ResourceUseVulkanIntegrationTest, ExternalViewFactoriesValidateUsageAlignmentAndCapacity) {
        vsl::Buffer<vsl::MemoryType::VERTEX_BUFFER, vsl::MemoryProperty::HOST_VISIBLE> vertex(device, 32);
        vsl::Buffer<vsl::MemoryType::UNIFORM_BUFFER, vsl::MemoryProperty::HOST_VISIBLE> uniform(device, 32);
        using VertexView = t::TypedVertexBufferView<float>;
        using UniformView = t::TypedResourceBufferView<float, vsl::graphic_resource::Type::UNIFORM_BUFFER>;
        EXPECT_THROW(VertexView::FromBuffer(uniform, 1), std::invalid_argument);
        EXPECT_THROW(UniformView::FromBuffer(vertex, 1), std::invalid_argument);
        EXPECT_THROW(VertexView::FromBuffer(vertex, 0), std::out_of_range);
        EXPECT_THROW(VertexView::FromBuffer(vertex, SIZE_MAX), std::out_of_range);
        EXPECT_THROW(VertexView::FromBuffer(vertex, 1, 1), std::invalid_argument);
        EXPECT_THROW(UniformView::FromBuffer(uniform, 1, SIZE_MAX), std::out_of_range);
        const auto view = UniformView::FromBuffer(uniform, 4, 8).view(1, 2);
        EXPECT_EQ(view.byte_offset(), 12u);
        EXPECT_EQ(view.size(), 2u);
    }

    TEST_F(ResourceUseVulkanIntegrationTest, RetainedFrameMayOwnItsResourceUseStateWithoutACycle) {
        struct Frame {
            vsl::ResourceUseState use;
            vsl::CompletionObservation update;
        };
        std::weak_ptr<Frame> owner;
        {
            auto manager = make_manager().independent(1);
            auto phase = manager.start_phase<vsl::ComputePhase>();
            auto frame = std::make_shared<Frame>();
            owner = frame;
            frame->use.mark_used(phase.completion());
            frame->update = vsl::CompletionObservation(phase.completion());
            phase.completion().retain(frame);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        EXPECT_TRUE(owner.expired());
    }

    TEST_F(ResourceUseVulkanIntegrationTest, ObservationsPreserveCompletedAndCancelledResultsAfterOwnersAreDestroyed) {
        vsl::CompletionObservation complete, cancelled;
        {
            auto manager = make_manager().independent(1);
            auto phase = manager.start_phase<vsl::ComputePhase>();
            complete = vsl::CompletionObservation(phase.completion());
            EXPECT_EQ(complete.status(), vsl::Completion::Status::PENDING);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        EXPECT_EQ(complete.status(), vsl::Completion::Status::COMPLETE);
        {
            auto manager = make_manager().independent(1);
            auto phase = manager.start_phase<vsl::ComputePhase>();
            cancelled = vsl::CompletionObservation(phase.completion());
            phase.cancel();
        }
        EXPECT_EQ(cancelled.status(), vsl::Completion::Status::CANCELLED);
    }
    TEST_F(ResourceUseVulkanIntegrationTest, RetainedFrameCanStoreItsOwnCompletionWithoutAWorkOwnershipCycle) {
        struct Frame {
            vsl::ResourceUseState use;
            vsl::Completion completion;
        };
        for (const bool cancel : {false, true}) {
            auto manager = make_manager();
            auto phase = manager.start_phase<vsl::ComputePhase>();
            std::weak_ptr<Frame> observed;
            const auto result = phase.completion();
            {
                auto frame = std::make_shared<Frame>();
                observed = frame;
                frame->completion = result;
                frame->use.mark_used(result);
                manager.get_buffer().retain_until_complete(frame);
            }
            EXPECT_FALSE(observed.expired());
            if (cancel)
                phase.cancel();
            else {
                ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                ASSERT_TRUE(result.wait());
            }
            EXPECT_TRUE(observed.expired());
            EXPECT_EQ(result.status(), cancel ? vsl::Completion::Status::CANCELLED : vsl::Completion::Status::COMPLETE);
        }
    }

    TEST_F(ResourceUseVulkanIntegrationTest, DescriptorUpdatesAfterBindingUseTheLatestResourcesAndFreezeDuringRecording) {
        auto manager = make_manager().independent(1);
        vsl::GraphicResourceManager descriptors(device);
        t::TypedDescriptorSet<Set> set(device, descriptors);
        using Native = vsl::Buffer<vsl::MemoryType::UNIFORM_BUFFER, vsl::MemoryProperty::HOST_VISIBLE>;
        const std::uint32_t a = 100, b = 200, zero = 0;
        Native first(device, &a), second(device, &b);
        auto old_use = std::make_shared<vsl::ResourceUseState>();
        auto new_use = std::make_shared<vsl::ResourceUseState>();
        using View = t::TypedResourceBufferView<std::uint32_t, vsl::graphic_resource::Type::UNIFORM_BUFFER>;
        auto old_view = View::FromBuffer(first, 1, 0, {old_use, BufferAccess::Owned(first)});
        auto new_view = View::FromBuffer(second, 1, 0, {new_use, BufferAccess::Owned(second)});
        t::TypedStorageBuffer<std::uint32_t> output(device, zero);
        set.write<0>(old_view);
        set.write<1>(output);
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_frame.comp.spv"));
        t::TypedComputePipeline<Schema> pipeline(device, vsl::pipeline_layout::ShaderGroup("tracked", {shader}));
        const auto command = pipeline.bind_resources(set).dispatch(1);
        set.write<0>(new_view);
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << command;
        EXPECT_FALSE(old_use->busy());
        EXPECT_TRUE(new_use->busy());
        EXPECT_THROW(set.write<0>(old_view), std::logic_error);
        EXPECT_THROW(output.copy(std::span(&zero, 1)), std::logic_error);
        auto alias = set;
        EXPECT_THROW(alias.write<0>(old_view), std::logic_error);
        auto buffer = manager.get_buffer();
        vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
        CommandAccess::Data(buffer)->command_buffers[0].pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        std::array<std::uint32_t, 1> actual{};
        output.read_into(actual);
        EXPECT_EQ(actual.front(), b);
        EXPECT_NO_THROW(set.write<0>(old_view));
        EXPECT_FALSE(new_use->busy());
    }

    TEST_F(ResourceUseVulkanIntegrationTest, NativeAndTypedDescriptorBindingsFreezeBothUpdateEntrances) {
        auto manager = make_manager().independent(1);
        vsl::GraphicResourceManager descriptors(device);
        t::TypedDescriptorSet<Set> set(device, descriptors);
        const std::uint32_t value = 100, zero = 0;
        t::TypedUniformBuffer<std::uint32_t> input(device, value);
        t::TypedStorageBuffer<std::uint32_t> output(device, zero);
        set.write<0>(input);
        set.write<1>(output);
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_frame.comp.spv"));
        t::TypedComputePipeline<Schema> pipeline(device, vsl::pipeline_layout::ShaderGroup("tracked", {shader}));
        auto native = set.backend();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << vsl::command::BindGraphicResource(native, vsl::graphic_resource::BindingDestination::COMPUTE, pipeline.backend());
            EXPECT_THROW(set.write<0>(input), std::logic_error);
            EXPECT_THROW(native.update(input.backend()), std::logic_error);
            phase.cancel();
        }
        EXPECT_NO_THROW(set.write<0>(input));
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << pipeline.bind_resources(set).dispatch(1);
        EXPECT_THROW(native.update(input.backend()), std::logic_error);
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        EXPECT_NO_THROW(native.update(input.backend()));
    }

    TEST_F(ResourceUseVulkanIntegrationTest, PartialNativeReplacementUpdatesLogicalConstraintsAndTracksEveryPhase) {
        using ArraySet = t::DescriptorSet<0, t::UniformBinding<3, std::uint32_t, vsl::ShaderFlag::COMPUTE, 2>>;
        using View = t::TypedResourceBufferView<std::uint32_t, vsl::graphic_resource::Type::UNIFORM_BUFFER>;
        vsl::GraphicResourceManager descriptors(device);
        t::TypedDescriptorSet<ArraySet> set(device, descriptors);
        const std::uint32_t value = 1;
        t::TypedUniformBuffer<std::uint32_t> a(device, value), b(device, value), replacement(device, value);
        auto first_constraint = std::make_shared<vsl::ResourceUseState>();
        auto second_constraint = std::make_shared<vsl::ResourceUseState>();
        set.write<3>(std::array{View::FromView(a.backend(), 1, 0, {first_constraint, {}}),
                                View::FromView(b.backend(), 1, 0, {second_constraint, {}})});
        auto native = set.backend();
        native.update(replacement.backend(), 3);
        const vsl::command::BindGraphicResource command(native, vsl::graphic_resource::BindingDestination::COMPUTE);
        auto manager = make_manager().independent(1), other = manager.independent(1);
        auto first = manager.start_phase<vsl::ComputePhase>();
        auto second = other.start_phase<vsl::ComputePhase>();
        // bind省略時と同じtrack入口で、それぞれの使用を登録する。
        command.track(manager.get_buffer());
        set.resource_use()->record(other.get_buffer());
        EXPECT_FALSE(first_constraint->busy());
        EXPECT_TRUE(second_constraint->busy());
        EXPECT_FALSE(a.backend().ref().use().busy());
        EXPECT_TRUE(b.backend().ref().use().busy());
        EXPECT_TRUE(replacement.backend().ref().use().busy());
        first.cancel();
        EXPECT_THROW(native.update(replacement.backend(), 3), std::logic_error);
        ASSERT_EQ(second.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(second.completion().wait());
        EXPECT_FALSE(second_constraint->busy());
        EXPECT_NO_THROW(native.update(replacement.backend(), 3));
    }

    TEST_F(ResourceUseVulkanIntegrationTest, BusyPoolDestroyKeepsManagerOwnershipAndCancellingAllowsDestruction) {
        for (const bool overallocate : {false, true}) {
            std::optional<vsl::graphic_resource::ExactAllocateManager> exact;
            std::optional<vsl::graphic_resource::OverAllocateManager> over;
            if (overallocate)
                over.emplace(device);
            else
                exact.emplace(device);
            auto& descriptors = overallocate ? static_cast<vsl::graphic_resource::ManagerInterface&>(*over)
                                             : static_cast<vsl::graphic_resource::ManagerInterface&>(*exact);
            t::TypedDescriptorSet<Set> set(device, descriptors);
            const std::uint32_t value = 1;
            t::TypedUniformBuffer<std::uint32_t> input(device, value);
            t::TypedStorageBuffer<std::uint32_t> output(device, value);
            set.write<0>(input);
            set.write<1>(output);
            auto native = set.backend();
            auto pool = native.get_pool();
            const auto count = descriptors.data_->pools.size();
            auto manager = make_manager().independent(1);
            auto phase = manager.start_phase<vsl::ComputePhase>();
            set.resource_use()->record(manager.get_buffer());
            EXPECT_THROW(descriptors.destroy(pool), std::logic_error);
            EXPECT_EQ(descriptors.data_->pools.size(), count);
            if (over)
                EXPECT_EQ(over->pools.front().data_, pool.data_);
            EXPECT_NO_THROW(set.require_complete());
            phase.cancel();
            EXPECT_NO_THROW(descriptors.destroy(pool));
            EXPECT_FALSE(native.data_->descriptor_set);
            EXPECT_TRUE(native.data_->bindings.empty());
            EXPECT_THROW(set.require_complete(), std::logic_error);
        }
    }

    TEST_F(ResourceUseVulkanIntegrationTest, DescriptorConstraintReplacementReleasesTheOldOwnerAndUsesTheNewState) {
        vsl::GraphicResourceManager descriptors(device);
        t::TypedDescriptorSet<Set> set(device, descriptors);
        const std::uint32_t value = 1;
        t::TypedUniformBuffer<std::uint32_t> input(device, value);
        t::TypedStorageBuffer<std::uint32_t> output(device, value);
        auto old_state = std::make_shared<vsl::ResourceUseState>(), new_state = std::make_shared<vsl::ResourceUseState>();
        std::weak_ptr<int> old_owner, new_owner;
        {
            auto owner = std::make_shared<int>(1);
            old_owner = owner;
            using View = t::TypedResourceBufferView<std::uint32_t, vsl::graphic_resource::Type::UNIFORM_BUFFER>;
            set.write<0>(View::FromView(input.backend(), 1, 0, {old_state, owner}));
        }
        set.write<1>(output);
        EXPECT_FALSE(old_owner.expired());
        {
            auto owner = std::make_shared<int>(2);
            new_owner = owner;
            set.resource_use()->replace(0, {{new_state, owner}});
        }
        EXPECT_TRUE(old_owner.expired());
        EXPECT_FALSE(new_owner.expired());
        auto manager = make_manager().independent(1);
        auto phase = manager.start_phase<vsl::ComputePhase>();
        set.resource_use()->record(manager.get_buffer());
        EXPECT_FALSE(old_state->busy());
        EXPECT_TRUE(new_state->busy());
        phase.cancel();
        auto native = set.backend();
        const auto layout = native.data_->layout;
        native.data_->layout = nullptr;
        EXPECT_THROW(set.resource_use()->replace(0, {{new_state, {}}}), std::logic_error);
        native.data_->layout = layout;
    }

    TEST_F(ResourceUseVulkanIntegrationTest, UseStatePollingAllowsReentryAndPreservesUsesAddedByCompletionCallbacks) {
        for (const bool wait : {false, true}) {
            auto manager = make_manager().independent(1), other = manager.independent(1), last = manager.independent(1);
            vsl::ResourceUseState state;
            bool released = false;
            auto submitted = manager.start_phase<vsl::ComputePhase>();
            auto peer = last.start_phase<vsl::ComputePhase>();
            state.mark_used(submitted.completion());
            manager.get_buffer().retain_until_complete(std::shared_ptr<int>(new int(1), [&](int* value) {
                delete value;
                released = true;
                EXPECT_EQ(state.busy(), !wait);
                state.mark_used(peer.completion());
                EXPECT_TRUE(state.busy());
                EXPECT_THROW(state.require_idle(), std::logic_error);
            }));
            ASSERT_EQ(submitted.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            device.data_->device.waitIdle();
            auto recording = other.start_phase<vsl::ComputePhase>();
            if (wait)
                EXPECT_FALSE(state.wait());
            else
                EXPECT_NO_THROW(state.mark_used(recording.completion()));
            EXPECT_TRUE(released);
            EXPECT_TRUE(state.busy());
            recording.cancel();
            EXPECT_TRUE(state.busy());
            peer.cancel();
            EXPECT_TRUE(state.wait(0));
        }
    }

    TEST_F(ResourceUseVulkanIntegrationTest, PoolRetirementRejectsReentryAndUpdatesRecheckValidityAfterIdleObservation) {
        for (const bool retire : {false, true}) {
            vsl::GraphicResourceManager descriptors(device);
            t::TypedDescriptorSet<Set> set(device, descriptors, vsl::ResourceRetention::COMPLETION);
            const std::uint32_t value = 1;
            t::TypedUniformBuffer<std::uint32_t> input(device, value);
            t::TypedStorageBuffer<std::uint32_t> output(device, value);
            set.write<0>(input);
            set.write<1>(output);
            auto native = set.backend();
            auto pool = native.get_pool();
            vsl::graphic_resource::BindingLayout layout(device, vsl::graphic_resource::Type::UNIFORM_BUFFER, vsl::ShaderFlag::COMPUTE);
            auto manager = make_manager().independent(1), other = manager.independent(1);
            bool released = false;
            auto phase = manager.start_phase<vsl::ComputePhase>();
            auto peer = other.start_phase<vsl::ComputePhase>();
            set.resource_use()->record(manager.get_buffer());
            manager.get_buffer().retain_until_complete(std::shared_ptr<int>(new int(1), [&](int* value) {
                delete value;
                released = true;
                if (retire) {
                    EXPECT_THROW(pool.bind(layout), std::logic_error);
                    EXPECT_THROW(set.resource_use()->record(other.get_buffer()), std::logic_error);
                    EXPECT_THROW(native.update(input.backend()), std::logic_error);
                    EXPECT_THROW(descriptors.destroy(pool), std::logic_error);
                } else
                    EXPECT_NO_THROW(descriptors.destroy(pool));
            }));
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            device.data_->device.waitIdle();
            if (retire)
                EXPECT_NO_THROW(descriptors.destroy(pool));
            else
                EXPECT_THROW(native.update(input.backend()), std::logic_error);
            EXPECT_TRUE(released);
            EXPECT_FALSE(pool.data_->descriptor_pool);
            EXPECT_FALSE(pool.data_->retiring);
            EXPECT_TRUE(native.data_->bindings.empty());
            peer.cancel();
        }
    }

    TEST_F(ResourceUseVulkanIntegrationTest, DescriptorTrackingRejectsCancellationInsidePhysicalUseObservation) {
        vsl::GraphicResourceManager descriptors(device);
        t::TypedDescriptorSet<Set> set(device, descriptors);
        const std::uint32_t value = 1;
        t::TypedUniformBuffer<std::uint32_t> input(device, value);
        t::TypedStorageBuffer<std::uint32_t> output(device, value);
        set.write<0>(input);
        set.write<1>(output);
        auto native = set.backend();
        auto pool = native.get_pool();
        auto manager = make_manager().independent(1), other = manager.independent(1);
        std::optional<vsl::ComputePhase> recording;
        bool released = false;
        auto submitted = manager.start_phase<vsl::ComputePhase>();
        input.backend().resource_use().record(manager.get_buffer());
        manager.get_buffer().retain_until_complete(std::shared_ptr<int>(new int(1), [&](int* value) {
            delete value;
            released = true;
            if (recording)
                recording->cancel();
            EXPECT_THROW(set.write<0>(input), std::logic_error);
            EXPECT_THROW(descriptors.destroy(pool), std::logic_error);
        }));
        ASSERT_EQ(submitted.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        device.data_->device.waitIdle();
        recording.emplace(other);
        EXPECT_THROW(set.resource_use()->record(other.get_buffer()), std::logic_error);
        EXPECT_TRUE(released);
        EXPECT_FALSE(set.backend().data_->recording);
        EXPECT_NO_THROW(set.write<0>(input));
    }

    TEST_F(ResourceUseVulkanIntegrationTest, DescriptorAliasesKeepAdditionalOwnersForTheGroupsRetentionPolicy) {
        vsl::GraphicResourceManager descriptors(device);
        vsl::graphic_resource::BindingLayout layout(device, vsl::graphic_resource::Type::UNIFORM_BUFFER, vsl::ShaderFlag::COMPUTE);
        auto [pools, allocated] = descriptors.allocate({layout});
        const auto native = allocated.front();
        t::TypedUniformBuffer<std::uint32_t> input(device, 17u);
        auto initialized = native;
        initialized.update(input.backend());
        struct Composite {
            vsl::GraphicResource native;
            std::shared_ptr<int> token;
        };
        for (const auto retention : {vsl::ResourceRetention::COMPLETION, vsl::ResourceRetention::SUBMISSION}) {
            for (const bool cancel : {false, true}) {
                std::weak_ptr<int> token;
                {
                    auto manager = make_manager().independent(1);
                    auto phase = manager.start_phase<vsl::ComputePhase>();
                    {
                        const auto group = vsl::ResourceUseGroup::ForDescriptor(1, native, retention);
                        const auto context = std::make_shared<Composite>(Composite{native, std::make_shared<int>(3)});
                        token = context->token;
                        const vsl::GraphicResource aliased{
                            std::shared_ptr<vsl::impl::GraphicResourceImpl>(context, native.data_.get())};
                        ASSERT_TRUE(group->tracks_descriptor(aliased));
                        vsl::command::BindGraphicResource command(aliased, vsl::graphic_resource::BindingDestination::COMPUTE,
                                                                  std::nullopt, 0, group);
                        command.track(manager.get_buffer());
                    }
                    EXPECT_FALSE(token.expired());
                    if (cancel)
                        phase.cancel();
                    else {
                        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                        ASSERT_TRUE(phase.completion().wait());
                    }
                    EXPECT_EQ(token.expired(), retention == vsl::ResourceRetention::COMPLETION);
                }
                EXPECT_TRUE(token.expired());
            }
        }
    }

    TEST_F(ResourceUseVulkanIntegrationTest, NativeUpdateKeepsItsTargetWhenCompletionDestroysTheCallingWrapper) {
        vsl::GraphicResourceManager descriptors(device);
        vsl::graphic_resource::BindingLayout layout(device, vsl::graphic_resource::Type::UNIFORM_BUFFER, vsl::ShaderFlag::COMPUTE);
        auto [pool, allocated] = descriptors.make(std::vector{layout});
        auto native = allocated.front();
        const std::weak_ptr<vsl::impl::GraphicResourceImpl> target = native.data_;
        allocated.clear();
        const std::uint32_t value = 1;
        t::TypedUniformBuffer<std::uint32_t> input(device, value);
        native.update(input.backend());
        auto manager = make_manager().independent(1);
        bool released = false;
        auto phase = manager.start_phase<vsl::ComputePhase>();
        vsl::ResourceUseGroup::ForDescriptor(1, native, vsl::ResourceRetention::COMPLETION)->record(manager.get_buffer());
        manager.get_buffer().retain_until_complete(std::shared_ptr<int>(new int(1), [&](int* value) {
            delete value;
            released = true;
            native = {};
        }));
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        device.data_->device.waitIdle();
        EXPECT_NO_THROW(native.update(input.backend()));
        EXPECT_TRUE(released);
        EXPECT_FALSE(native.data_);
        EXPECT_TRUE(target.expired());
    }

    TEST_F(ResourceUseVulkanIntegrationTest, DescriptorRecordingKeepsTheSetWhenCallbackDestroysTheGroupAndCancelsItsPins) {
        vsl::GraphicResourceManager descriptors(device);
        vsl::graphic_resource::BindingLayout layout(device, vsl::graphic_resource::Type::UNIFORM_BUFFER, vsl::ShaderFlag::COMPUTE);
        auto [pool, allocated] = descriptors.make(std::vector{layout});
        const std::weak_ptr<vsl::impl::GraphicResourceImpl> target = allocated.front().data_;
        const std::uint32_t value = 1;
        t::TypedUniformBuffer<std::uint32_t> input(device, value);
        allocated.front().update(input.backend());
        auto group = vsl::ResourceUseGroup::ForDescriptor(1, allocated.front(), vsl::ResourceRetention::COMPLETION);
        allocated.clear();
        auto manager = make_manager().independent(1), other = manager.independent(1);
        std::optional<vsl::ComputePhase> recording;
        bool released = false;
        auto submitted = manager.start_phase<vsl::ComputePhase>();
        input.backend().resource_use().record(manager.get_buffer());
        manager.get_buffer().retain_until_complete(std::shared_ptr<int>(new int(1), [&](int* value) {
            delete value;
            released = true;
            group.reset();
            if (recording)
                recording->cancel();
        }));
        ASSERT_EQ(submitted.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        device.data_->device.waitIdle();
        recording.emplace(other);
        EXPECT_THROW(group->record(other.get_buffer()), std::logic_error);
        EXPECT_TRUE(released);
        EXPECT_FALSE(group);
        EXPECT_TRUE(target.expired());
    }

    TEST_F(TrackedDrawVulkanIntegrationTest, BatchTracksEveryVertexOwnerEvenWhenItsBindIsSkipped) {
        vsl::Buffer<vsl::MemoryType::VERTEX_BUFFER, vsl::MemoryProperty::HOST_VISIBLE> native(device, VERTICES);
        auto first = std::make_shared<vsl::ResourceUseState>();
        auto second = std::make_shared<vsl::ResourceUseState>();
        using View = t::TypedVertexBufferView<Vertex>;
        const auto first_view = View::FromBuffer(native, 3, 0, {first, BufferAccess::Owned(native)});
        const auto second_view = View::FromBuffer(native, 3, 0, {second, BufferAccess::Owned(native)});
        namespace pl = vsl::pipeline_layout;
        vsl::Shader<vsl::ShaderType::VERTEX> source(device, vsl_test::shader_path("vertices.vert.spv"));
        t::TypedGraphicsPipeline<Input> pipeline(device, *pass, pl::ShaderGroup("tracked vertices", {source, *fragment}),
                                                 pl::ColorBlend{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{}, pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description));
        auto batch = t::TypedBatch(pipeline.bind_vertices(first_view).draw(3), pipeline.bind_vertices(second_view).draw(3));
        EXPECT_EQ(batch.statistics().vertex_binds, 1u);
        ExpectTriangleColor(render([&](auto& stream) {
            stream << batch;
            EXPECT_TRUE(first->busy());
            EXPECT_TRUE(second->busy());
        }));
        EXPECT_FALSE(first->busy());
        EXPECT_FALSE(second->busy());
    }

    TEST_F(TrackedDrawVulkanIntegrationTest, IndexedDrawBlocksOwnedVertexAndIndexUpdatesUntilCancellation) {
        t::TypedVertexBuffer<Vertex> vertices(device, VERTICES);
        const std::array<std::uint16_t, 3> values{0, 1, 2};
        t::TypedIndexBuffer<std::uint16_t> indices(device, values);
        namespace pl = vsl::pipeline_layout;
        vsl::Shader<vsl::ShaderType::VERTEX> source(device, vsl_test::shader_path("vertices.vert.spv"));
        t::TypedGraphicsPipeline<Input> pipeline(device, *pass, pl::ShaderGroup("tracked indices", {source, *fragment}),
                                                 pl::ColorBlend{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{}, pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description));
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << vsl::command::RenderPassBegin(*pass, *framebuffer) << vsl::Viewport(description) << vsl::Scissor(description)
              << pipeline.bind_vertices(vertices).draw_indexed(indices, 3) << vsl::command::RenderPassEnd{};
        EXPECT_THROW(vertices.copy(VERTICES), std::logic_error);
        EXPECT_THROW(indices.copy(values), std::logic_error);
        phase.cancel();
        EXPECT_NO_THROW(vertices.copy(VERTICES));
        EXPECT_NO_THROW(indices.copy(values));
    }
} // namespace
