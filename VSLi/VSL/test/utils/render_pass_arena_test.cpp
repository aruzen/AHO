#include <VSL/lifetime/graphics_access.hpp>
#include "support/render_fixture.hpp"
#include "support/read_image.hpp"
#include "support/frame_buffer_metadata.hpp"
#include <VSL/lifetime/resource_arena.hpp>
#include <VSL/utils/typed_pipeline.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::detail::GraphicsAccess;
}

namespace {
    namespace t = vsl::typed;
    namespace s = vsl::storage;
    namespace l = vsl::lifetime;
    constexpr std::array<t::ResourceRequest, 2> REQUESTS{{{{1}, s::ResourceKind::RENDER_PASS}, {{2}, s::ResourceKind::RENDER_PASS}}};
    constexpr t::ResourcePlan<2> PLAN(REQUESTS);
    class RenderPassArenaVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    vsl::LogicalDeviceAccessor another_device(vsl::PhysicalDevice physical, std::uint32_t family);
    template<typename T>
    concept temporary_pass_description = requires(T&& value) { std::move(value).description(); };
    static_assert(!temporary_pass_description<vsl::RenderPass>);
    static_assert(!temporary_pass_description<vsl::RenderPassAccessor>);
    static_assert(!temporary_pass_description<vsl::IDPickingRenderPass>);
    static_assert(!std::is_copy_constructible_v<vsl::impl::RenderPassImpl>);
    static_assert(!std::is_move_constructible_v<vsl::impl::RenderPassImpl>);
    vsl::LogicalDeviceAccessor another_device(vsl::PhysicalDevice physical, std::uint32_t family) {
        vsl::DeviceCreateInfo info;
        info.graphics_family = family;
        return vsl::LogicalDeviceAccessor::Create(std::move(physical), std::move(info));
    }
    TEST(RenderPassPlacementTest, ExactSizeAndAlignmentIdentifyDistinctSlotsWithoutGpuUseState) {
        static_assert(s::ResourceSlotLayout<vsl::impl::RenderPassImpl>::USE_OFFSET == s::NO_SEPARATE_USE);
        static_assert(s::ResourceSlotLayout<vsl::impl::RenderPassImpl>::SIZE == s::RENDER_PASS_IMPL_SIZE);
        static_assert(PLAN.placement({1}).offset == 0);
        static_assert(PLAN.placement({2}).offset == s::RENDER_PASS_IMPL_SIZE);
        EXPECT_EQ(PLAN.slots(), 2u);
        EXPECT_EQ(PLAN.bytes(), 2 * s::RENDER_PASS_IMPL_SIZE);
        EXPECT_EQ(PLAN.placement({2}).offset % s::RENDER_PASS_IMPL_ALIGNMENT, 0u);
    }
    TEST(RenderPassPlacementTest, EmptyFacadeAndMissingDeviceAreRejected) {
        const vsl::RenderPassAccessor empty;
        EXPECT_EQ(empty.owner_ref().use_count(), 0);
        EXPECT_THROW(empty.description(), std::logic_error);
        EXPECT_THROW(empty.get_device(), std::invalid_argument);
        vsl::LogicalDeviceAccessor missing;
        EXPECT_THROW((vsl::RenderPass(missing, vsl::RenderPassDescription{})), std::invalid_argument);
        missing.data_ = std::make_shared<vsl::impl::LogicalDeviceImpl>();
        EXPECT_THROW((vsl::RenderPass(missing, vsl::RenderPassDescription{})), std::invalid_argument);
    }
    TEST(RenderPassPlacementTest, MissingDescriptionIsRejectedByBothClearConstructors) {
        const auto target = vsl_test::metadata_frame_buffer(std::make_shared<vsl::impl::RenderPassImpl>());
        EXPECT_THROW((vsl::command::RenderPassBegin(target, std::vector<vsl::ClearValue>{})), std::logic_error);
        EXPECT_THROW((vsl::command::RenderPassBegin::WithClears(target, {})), std::logic_error);
    }
    TEST_F(RenderPassArenaVulkanIntegrationTest, SlotsOwnIndependentDescriptionsAndTheDeviceGetterKeepsOnlyTheDevice) {
        std::weak_ptr<void> owner;
        std::optional<vsl::RenderPassAccessor> kept;
        vsl::LogicalDeviceAccessor device_only;
        {
            auto info = pass->description();
            std::array<l::ResourceInit, 2> inputs{info, info};
            l::ResourceArena arena(device, PLAN, inputs);
            owner = arena.owner();
            const auto first = arena.render_pass({1}), second = arena.render_pass({2});
            EXPECT_NE(GraphicsAccess::Owned(first)->render_pass, GraphicsAccess::Owned(second)->render_pass);
            EXPECT_NE(&first.description(), &second.description());
            const auto base = reinterpret_cast<std::uintptr_t>(GraphicsAccess::Owned(first).get());
            EXPECT_EQ(reinterpret_cast<std::uintptr_t>(GraphicsAccess::Owned(second).get()) - base, PLAN.placement({2}).offset);
            const auto metadata = reinterpret_cast<std::uintptr_t>(&first.description());
            EXPECT_GE(metadata, base);
            EXPECT_LT(metadata, base + s::RENDER_PASS_IMPL_SIZE);
            EXPECT_FALSE(GraphicsAccess::Owned(first).owner_before(GraphicsAccess::Owned(second)));
            EXPECT_FALSE(GraphicsAccess::Owned(second).owner_before(GraphicsAccess::Owned(first)));
            EXPECT_TRUE(first.owner_ref().managed());
            std::get<vsl::RenderPassDescription>(inputs[0]).attachments.clear();
            info.attachments.clear();
            EXPECT_EQ(first.description().attachments.size(), 1u);
            EXPECT_EQ(second.description().attachments.size(), 1u);
            device_only = first.get_device();
            EXPECT_EQ(device_only.data_, device.data_);
            EXPECT_FALSE(device_only.data_.owner_before(device.data_));
            EXPECT_FALSE(device.data_.owner_before(device_only.data_));
            EXPECT_THROW(arena.render_pass({99}), std::out_of_range);
            EXPECT_THROW(arena.buffer({1}), std::invalid_argument);
            kept = first;
        }
        EXPECT_FALSE(owner.expired());
        EXPECT_EQ(kept->description().attachments.size(), 1u);
        kept.reset();
        EXPECT_TRUE(owner.expired());
        EXPECT_EQ(device_only.data_, device.data_);
    }
    TEST_F(RenderPassArenaVulkanIntegrationTest, ImportsKeepTheOriginalOwnerAndRejectNonownersOrAnotherDevice) {
        constexpr std::array<t::ResourceRequest, 2> requests{{{{1}, s::ResourceKind::RENDER_PASS, true}, {{2}, s::ResourceKind::BUFFER}}};
        constexpr t::ResourcePlan<2> plan(requests);
        std::weak_ptr<void> original, importer;
        vsl::RenderPassAccessor kept;
        {
            vsl::RenderPass source(device, pass->description());
            original = GraphicsAccess::Owned(source).share();
            const std::array<l::ResourceInit, 2> inputs{vsl::RenderPassAccessor{source}, vsl::BufferCreateInfo{4, vsl::MemoryType::TRANSFER_SOURCE}};
            {
                l::ResourceArena arena(device, plan, inputs);
                importer = arena.owner();
                kept = arena.render_pass({1});
                EXPECT_EQ(GraphicsAccess::Owned(kept), GraphicsAccess::Owned(source));
                EXPECT_FALSE(GraphicsAccess::Owned(kept).owner_before(GraphicsAccess::Owned(source)));
                EXPECT_FALSE(GraphicsAccess::Owned(source).owner_before(GraphicsAccess::Owned(kept)));
            }
            EXPECT_TRUE(importer.expired());
            const auto other = another_device(physical, queue_family);
            EXPECT_THROW((l::ResourceArena(other, plan, inputs)), std::invalid_argument);
            for (const bool borrowed : {false, true}) {
                const auto invalid = GraphicsAccess::FromShared(borrowed ? std::shared_ptr<vsl::impl::RenderPassImpl>(std::shared_ptr<void>{}, GraphicsAccess::Owned(source).get()) : nullptr);
                const std::array<l::ResourceInit, 2> rejected{invalid, vsl::BufferCreateInfo{4, vsl::MemoryType::TRANSFER_SOURCE}};
                EXPECT_THROW((l::ResourceArena(device, plan, rejected)), std::invalid_argument);
            }
        }
        EXPECT_FALSE(original.expired());
        kept = {};
        EXPECT_TRUE(original.expired());
    }
    TEST_F(RenderPassArenaVulkanIntegrationTest, LaterFailureRollsBackTheSameRegionWithoutDestroyingImports) {
        constexpr std::array<t::ResourceRequest, 3> requests{{{{1}, s::ResourceKind::RENDER_PASS}, {{2}, s::ResourceKind::RENDER_PASS}, {{3}, s::ResourceKind::RENDER_PASS, true}}};
        constexpr t::ResourcePlan<3> plan(requests);
        s::ArenaMemory memory(plan.bytes(), plan.alignment());
        const auto valid = pass->description();
        auto invalid = valid;
        invalid.subpasses[0].colors[0].attachment = 99;
        std::array<l::ResourceInit, 3> inputs{valid, invalid, vsl::RenderPassAccessor{*pass}};
        const auto before = GraphicsAccess::Owned(*(pass)).use_count();
        for (int attempt = 0; attempt < 3; ++attempt) {
            EXPECT_THROW((l::ResourceArena(device, plan, inputs, memory)), std::out_of_range);
            EXPECT_EQ(GraphicsAccess::Owned(*(pass)).use_count(), before);
            EXPECT_TRUE(GraphicsAccess::Owned(*(pass))->render_pass);
        }
        inputs[1] = valid;
        EXPECT_NO_THROW((l::ResourceArena(device, plan, inputs, memory)));
        vsl::RenderPassDescription too_many;
        const auto limit = physical.data_->device.getProperties().limits.maxColorAttachments;
        too_many.subpasses[0].colors.assign(limit + 1, {vsl::UNUSED_ATTACHMENT});
        inputs[1] = too_many;
        EXPECT_THROW((l::ResourceArena(device, plan, inputs, memory)), std::out_of_range);
        inputs[1] = valid;
        EXPECT_NO_THROW((l::ResourceArena(device, plan, inputs, memory)));
    }
    TEST_F(RenderPassArenaVulkanIntegrationTest, SavedTypedDrawUsesThePlacedPassUntilGpuCompletionAndSlotReplacement) {
        using Input = t::TypedVertexInput<>;
        using Draw = t::TypedDraw<Input, t::ResourceSchema<>, t::PushSchema<>>;
        constexpr std::array<t::ResourceRequest, 1> requests{{{{1}, s::ResourceKind::RENDER_PASS}}};
        constexpr t::ResourcePlan<1> plan(requests);
        std::weak_ptr<void> owner;
        std::optional<vsl::command::RenderPassBegin> begin;
        std::optional<Draw> draw;
        {
            const std::array<l::ResourceInit, 1> inputs{pass->description()};
            l::ResourceArena arena(device, plan, inputs);
            owner = arena.owner();
            const auto placed = arena.render_pass({1});
            vsl::FrameBuffer target(placed, WIDTH, HEIGHT, {*color});
            namespace pl = vsl::pipeline_layout;
            t::TypedGraphicsPipeline<Input> selected(device, placed, pl::ShaderGroup("placed pass", {*vertex, *fragment}),
                                                     pl::ColorBlend{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{},
                                                     vsl::Viewport(description), vsl::Scissor(description), pl::DynamicState{});
            begin.emplace(target, std::vector<vsl::ClearValue>{vsl::ClearColor{}});
            draw.emplace(selected.bind_vertices().draw(3));
        }
        EXPECT_FALSE(owner.expired());
        auto manager = make_manager();
        vsl::Completion completion;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            {
                auto stream = phase << *begin;
                stream << vsl::Viewport(description) << vsl::Scissor(description) << *draw << vsl::command::RenderPassEnd{};
            }
            begin.reset();
            draw.reset();
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            completion = phase.completion();
            ASSERT_TRUE(completion.wait());
            EXPECT_FALSE(owner.expired());
        }
        const auto bytes = vsl_test::read_image(manager, *color, vsl::ImageLayout::GENERAL);
        std::array<std::uint8_t, 4> pixel{};
        std::memcpy(pixel.data(), bytes.data() + ((HEIGHT / 2) * WIDTH + WIDTH / 2) * 4, 4);
        ExpectTriangleColor(pixel);
        {
            auto recycle = manager.start_phase<vsl::ComputePhase>();
            ASSERT_EQ(recycle.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(recycle.completion().wait());
        }
        EXPECT_TRUE(owner.expired());
        EXPECT_EQ(completion.status(), vsl::Completion::Status::COMPLETE);
    }
    TEST_F(RenderPassArenaVulkanIntegrationTest, CancelReleasesThePassAfterTheRecordedWorkOwnerIsGone) {
        constexpr std::array<t::ResourceRequest, 1> requests{{{{1}, s::ResourceKind::RENDER_PASS}}};
        constexpr t::ResourcePlan<1> plan(requests);
        auto manager = make_manager();
        std::weak_ptr<void> owner;
        vsl::Completion completion;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            completion = phase.completion();
            {
                const std::array<l::ResourceInit, 1> inputs{pass->description()};
                l::ResourceArena arena(device, plan, inputs);
                owner = arena.owner();
                vsl::FrameBuffer target(arena.render_pass({1}), WIDTH, HEIGHT, {*color});
                phase << vsl::command::RenderPassBegin(target, std::vector<vsl::ClearValue>{vsl::ClearColor{}})
                      << vsl::command::RenderPassEnd{};
            }
            EXPECT_FALSE(owner.expired());
            phase.cancel();
            EXPECT_FALSE(owner.expired());
        }
        EXPECT_TRUE(owner.expired());
        EXPECT_EQ(completion.status(), vsl::Completion::Status::CANCELLED);
    }
    TEST_F(RenderPassArenaVulkanIntegrationTest, ImportIsAFixedManagedParentEvenWithoutAGpuUseState) {
        using Set = t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
        using View = t::TypedResourceBufferView<std::uint32_t, vsl::graphic_resource::Type::UNIFORM_BUFFER>;
        constexpr std::array<t::ResourceRequest, 5> source_requests{{{{1}, s::ResourceKind::RENDER_PASS},
                                                                     {{2}, s::ResourceKind::BUFFER},
                                                                     {{3}, s::ResourceKind::DESCRIPTOR_LAYOUT},
                                                                     {{4}, s::ResourceKind::DESCRIPTOR_POOL},
                                                                     {{5}, s::ResourceKind::DESCRIPTOR_SET}}};
        constexpr std::array<t::ResourceRequest, 2> target_requests{{{{1}, s::ResourceKind::RENDER_PASS, true}, {{2}, s::ResourceKind::BUFFER}}};
        constexpr t::ResourcePlan<5> source_plan(source_requests);
        constexpr t::ResourcePlan<2> target_plan(target_requests);
        const vsl::DescriptorLayoutCreateInfo layout{{Set::POINTS.begin(), Set::POINTS.end()}};
        const vsl::DescriptorPoolCreateInfo pool{{{vsl::graphic_resource::Type::UNIFORM_BUFFER, 1}}, 1};
        const vsl::BufferCreateInfo buffer{4, vsl::MemoryType::UNIFORM_BUFFER};
        std::weak_ptr<void> a, b;
        {
            const std::array<l::ResourceInit, 5> inputs{pass->description(), buffer, layout, pool, l::DescriptorSetInit{{4}, {3}}};
            l::ResourceArena source(device, source_plan, inputs, l::DependencyPolicy::DYNAMIC);
            const std::array<l::ResourceInit, 2> imports{source.render_pass({1}), buffer};
            l::ResourceArena target(device, target_plan, imports, l::DependencyPolicy::DYNAMIC);
            a = source.owner();
            b = target.owner();
            auto set = t::TypedDescriptorSet<Set>::FromResource(device, source.descriptor_set({5}));
            set.write<0>(View::FromView(source.buffer({2}), 1));
            EXPECT_THROW(set.write<0>(View::FromView(target.buffer({2}), 1)), std::invalid_argument);
            EXPECT_NO_THROW(set.require_complete());
        }
        EXPECT_TRUE(a.expired());
        EXPECT_TRUE(b.expired());
    }
} // namespace
