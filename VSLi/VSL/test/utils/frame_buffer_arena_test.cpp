#include <VSL/lifetime/command_access.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include "support/render_fixture.hpp"
#include "support/fixed_parent.hpp"
#include "support/read_image.hpp"
#include "support/deferred_action.hpp"
#include <VSL/lifetime/resource_arena.hpp>
#include <VSL/lifetime/frame_buffer_storage.hpp>
#include <VSL/utils/typed_pipeline.hpp>
#include <functional>
#include <ranges>

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace {
    namespace t = vsl::typed;
    namespace l = vsl::lifetime;
    namespace s = vsl::storage;
    namespace pl = vsl::pipeline_layout;
    constexpr std::array<t::ResourceRequest, 1> REQUESTS{{{{1}, s::ResourceKind::FRAME_BUFFER}}};
    constexpr t::ResourcePlan<1> PLAN(REQUESTS);
    using vsl_test::DeferredAction;
    struct NativeFailure {
        size_t fail_at = 0, calls = 0;
        std::vector<vk::Framebuffer> created, destroyed;
        explicit NativeFailure(size_t fail_at);
        static vk::Framebuffer Create(void* context, vk::Device device, const vk::FramebufferCreateInfo& info);
        static void Destroy(void* context, vk::Device device, vk::Framebuffer framebuffer) noexcept;
    };
    class FrameBufferArenaVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
    protected:
        bool validate_vulkan() const override { return true; }
        l::FrameBufferInit input() const;
        void prime_callback(vsl::CommandManager& manager, const DeferredAction& action);
        void expect_clear(vsl::CommandManager manager, vsl::ImageAccessor image, std::array<std::uint8_t, 4> expected);
    };
    NativeFailure::NativeFailure(size_t fail_at) : fail_at(fail_at) {
        created.reserve(4);
        destroyed.reserve(4);
    }
    vk::Framebuffer NativeFailure::Create(void* context, vk::Device device, const vk::FramebufferCreateInfo& info) {
        auto& value = *static_cast<NativeFailure*>(context);
        if (++value.calls == value.fail_at)
            throw std::runtime_error("Injected native framebuffer failure");
        const auto framebuffer = device.createFramebuffer(info);
        value.created.push_back(framebuffer);
        return framebuffer;
    }
    void NativeFailure::Destroy(void* context, vk::Device device, vk::Framebuffer framebuffer) noexcept {
        auto& value = *static_cast<NativeFailure*>(context);
        value.destroyed.push_back(framebuffer);
        device.destroyFramebuffer(framebuffer);
    }
    l::FrameBufferInit FrameBufferArenaVulkanIntegrationTest::input() const {
        return {vsl::RenderPassAccessor{*pass}, WIDTH, HEIGHT, 1, {{vsl::ImageAccessor{*color}}}};
    }
    void FrameBufferArenaVulkanIntegrationTest::prime_callback(vsl::CommandManager& manager, const DeferredAction& action) {
        auto hook = action.owner();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << vsl::command::RenderPassBegin(*framebuffer) << vsl::command::RenderPassEnd{};
        manager.get_buffer().retain_until_complete(hook);
        hook.reset();
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        // 結果をpollせず、次のImage使用登録で完了callbackを実行させる。
        device.data_->device.waitIdle();
    }
    void FrameBufferArenaVulkanIntegrationTest::expect_clear(vsl::CommandManager manager, vsl::ImageAccessor image, std::array<std::uint8_t, 4> expected) {
        const auto bytes = vsl_test::read_image(manager, image, vsl::ImageLayout::GENERAL);
        ASSERT_EQ(bytes.size(), WIDTH * HEIGHT * 4);
        for (size_t i = 0; i < bytes.size(); ++i)
            EXPECT_EQ(std::to_integer<std::uint8_t>(bytes[i]), expected[i % 4]);
    }
    TEST(FrameBufferPlacementTest, SlotIncludesLifetimeContextAndSatisfiesTheExactLayout) {
        static_assert(s::ResourceSlotLayout<vsl::impl::FrameBufferImpl>::USE_OFFSET == s::NO_SEPARATE_USE);
        static_assert(std::is_standard_layout_v<l::FrameBufferSlotStorage>);
        static_assert(offsetof(l::FrameBufferSlotStorage, value) == 0);
        static_assert(!std::is_copy_constructible_v<vsl::impl::FrameBufferImpl>);
        static_assert(!std::is_move_constructible_v<vsl::impl::FrameBufferImpl>);
        EXPECT_EQ(PLAN.bytes(), s::FRAME_BUFFER_SLOT_SIZE);
        EXPECT_EQ(PLAN.alignment(), s::FRAME_BUFFER_SLOT_ALIGNMENT);
        EXPECT_GT(PLAN.bytes(), s::FRAME_BUFFER_IMPL_SIZE);
        EXPECT_THROW(PLAN.require_region(s::FRAME_BUFFER_IMPL_SIZE, PLAN.alignment()), std::invalid_argument);
        EXPECT_THROW(PLAN.require_region(PLAN.bytes(), PLAN.alignment() / 2), std::invalid_argument);
    }
    TEST(FrameBufferPlacementTest, EmptyFacadeHasNoFramesOrOwnerAndRejectsGetters) {
        const vsl::FrameBufferAccessor empty;
        EXPECT_EQ(empty.frame_count(), 0u);
        EXPECT_EQ(empty.owner_ref().use_count(), 0);
        EXPECT_THROW(empty.get_device(), std::invalid_argument);
        EXPECT_THROW(empty.get_render_pass(), std::invalid_argument);
    }
    TEST_F(FrameBufferArenaVulkanIntegrationTest, HeapGraphicsResourcesExposeTheirDirectDeviceEdge) {
        vsl_test::expect_fixed_parent(pipeline->owner_ref(), device.owner_ref());
        vsl_test::expect_fixed_parent(framebuffer->owner_ref(), device.owner_ref());
    }
    TEST_F(FrameBufferArenaVulkanIntegrationTest, MixedSlotsAndRowsShareOneOwnerAndUseTheDeclaredImages) {
        constexpr std::array<t::ResourceRequest, 5> requests{{{{1}, s::ResourceKind::FRAME_BUFFER}, {{2}, s::ResourceKind::FRAME_BUFFER}, {{3}, s::ResourceKind::RENDER_PASS}, {{4}, s::ResourceKind::IMAGE}, {{5}, s::ResourceKind::IMAGE}}};
        constexpr t::ResourcePlan<5> plan(requests);
        const vsl::ImageCreateInfo image{vsl::ImageType::COLOR_ATTACHMENT | vsl::ImageType::TRANSFER_SRC, WIDTH, HEIGHT, color->get_format()};
        const std::array<l::ResourceInit, 5> inputs{l::FrameBufferInit{t::ResourceId{3}, WIDTH, HEIGHT, 1, {{t::ResourceId{4}}, {t::ResourceId{5}}}},
                                                    l::FrameBufferInit{t::ResourceId{3}, WIDTH, HEIGHT, 1, {{t::ResourceId{5}}}}, pass->description(), image, image};
        l::ResourceArena arena(device, plan, inputs);
        const auto first = arena.frame_buffer({1}), second = arena.frame_buffer({2});
        const auto base = reinterpret_cast<std::uintptr_t>(first.data_.get());
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(second.data_.get()) - base, plan.placement({2}).offset);
        const auto context = reinterpret_cast<std::uintptr_t>(first.data_->owner_context);
        EXPECT_GE(context, base + s::FRAME_BUFFER_IMPL_SIZE);
        EXPECT_LT(context, base + s::FRAME_BUFFER_SLOT_SIZE);
        EXPECT_EQ(first.frame_count(), 2u);
        EXPECT_EQ(second.frame_count(), 1u);
        EXPECT_NE(first.data_->swap_chain_framebuffers[0], first.data_->swap_chain_framebuffers[1]);
        EXPECT_EQ(first.data_->attachments[1][0], arena.image({5}).data_.get());
        EXPECT_EQ(first.get_render_pass().data_, arena.render_pass({3}).data_);
        EXPECT_TRUE(first.owner_ref().managed());
        EXPECT_FALSE(first.data_.owner_before(second.data_));
        EXPECT_FALSE(second.data_.owner_before(first.data_));
        EXPECT_THROW(arena.frame_buffer({3}), std::invalid_argument);
        EXPECT_THROW(arena.frame_buffer({99}), std::out_of_range);
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << vsl::command::RenderPassBegin(first, {vsl::ClearColor{1, 0, 0, 1}}, {}, 0) << vsl::command::RenderPassEnd{}
                  << vsl::command::RenderPassBegin(first, {vsl::ClearColor{0, 1, 0, 1}}, {}, 1) << vsl::command::RenderPassEnd{};
            EXPECT_TRUE(arena.image({4}).use_state().busy());
            const auto root = arena.owner();
            const auto& owners = CommandAccess::Data(manager.get_buffer())->recording_context->work->resources;
            EXPECT_EQ(std::ranges::count_if(owners, [&](const auto& owner) { return !owner.owner_before(root) && !root.owner_before(owner); }), 1);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        expect_clear(manager, arena.image({4}), {255, 0, 0, 255});
        expect_clear(manager, arena.image({5}), {0, 255, 0, 255});
    }
    TEST_F(FrameBufferArenaVulkanIntegrationTest, ExternalGetterKeepsOnlyTheOriginalPassAndDeviceOwners) {
        std::weak_ptr<void> target_owner, image_owner, pass_owner;
        vsl::RenderPassAccessor kept;
        vsl::LogicalDeviceAccessor device_only;
        {
            vsl::RenderPass source(device, pass->description());
            ColorImage image(device, WIDTH, HEIGHT, color->get_format());
            image_owner = image.data_;
            pass_owner = source.data_;
            const std::array<l::ResourceInit, 1> inputs{l::FrameBufferInit{vsl::RenderPassAccessor{source}, WIDTH, HEIGHT, 1, {{vsl::ImageAccessor{image}}}}};
            l::ResourceArena arena(device, PLAN, inputs);
            target_owner = arena.owner();
            auto target = arena.frame_buffer({1});
            kept = target.get_render_pass();
            EXPECT_EQ(kept.data_, source.data_);
            EXPECT_FALSE(kept.data_.owner_before(source.data_));
            EXPECT_FALSE(source.data_.owner_before(kept.data_));
            device_only = target.get_device();
            EXPECT_EQ(device_only.data_, device.data_);
            EXPECT_FALSE(device_only.data_.owner_before(device.data_));
            EXPECT_FALSE(device.data_.owner_before(device_only.data_));
        }
        EXPECT_TRUE(target_owner.expired());
        EXPECT_TRUE(image_owner.expired());
        EXPECT_FALSE(pass_owner.expired());
        kept = {};
        EXPECT_TRUE(pass_owner.expired());
        EXPECT_EQ(device_only.data_, device.data_);
    }
    TEST_F(FrameBufferArenaVulkanIntegrationTest, DirectPassAndImageEdgesAndFramebufferImportsRejectReverseDependencies) {
        using Set = t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::FRAGMENT>>;
        using View = t::TypedResourceBufferView<std::uint32_t, vsl::graphic_resource::Type::UNIFORM_BUFFER>;
        constexpr std::array<t::ResourceRequest, 6> requests{{{{1}, s::ResourceKind::RENDER_PASS}, {{2}, s::ResourceKind::IMAGE}, {{3}, s::ResourceKind::BUFFER}, {{4}, s::ResourceKind::DESCRIPTOR_LAYOUT}, {{5}, s::ResourceKind::DESCRIPTOR_POOL}, {{6}, s::ResourceKind::DESCRIPTOR_SET}}};
        constexpr t::ResourcePlan<6> plan(requests);
        constexpr std::array<t::ResourceRequest, 2> targets{{{{1}, s::ResourceKind::FRAME_BUFFER}, {{2}, s::ResourceKind::BUFFER}}};
        constexpr t::ResourcePlan<2> target_plan(targets);
        constexpr std::array<t::ResourceRequest, 2> imports{{{{1}, s::ResourceKind::FRAME_BUFFER, true}, {{2}, s::ResourceKind::BUFFER}}};
        constexpr t::ResourcePlan<2> import_plan(imports);
        const vsl::BufferCreateInfo buffer{4, vsl::MemoryType::UNIFORM_BUFFER};
        const vsl::DescriptorLayoutCreateInfo binding{{Set::POINTS.begin(), Set::POINTS.end()}};
        const vsl::DescriptorPoolCreateInfo pool{{{vsl::graphic_resource::Type::UNIFORM_BUFFER, 1}}, 1};
        const vsl::ImageCreateInfo image{vsl::ImageType::COLOR_ATTACHMENT | vsl::ImageType::TRANSFER_SRC, WIDTH, HEIGHT, color->get_format()};
        const std::array<l::ResourceInit, 6> source_inputs{pass->description(), image, buffer, binding, pool, l::DescriptorSetInit{{5}, {4}}};
        for (const bool pass_parent : {false, true}) {
            std::weak_ptr<void> a, b, c;
            {
                l::ResourceArena source(device, plan, source_inputs, l::DependencyPolicy::DYNAMIC);
                a = source.owner();
                const auto selected_pass = pass_parent ? source.render_pass({1}) : vsl::RenderPassAccessor{*pass};
                const auto selected_image = pass_parent ? vsl::ImageAccessor{*color} : source.image({2});
                const std::array<l::ResourceInit, 2> target_inputs{l::FrameBufferInit{selected_pass, WIDTH, HEIGHT, 1, {{selected_image}}}, buffer};
                l::ResourceArena target(device, target_plan, target_inputs);
                b = target.owner();
                auto set = t::TypedDescriptorSet<Set>::FromResource(device, source.descriptor_set({6}));
                set.write<0>(View::FromView(source.buffer({3}), 1));
                EXPECT_THROW(set.write<0>(View::FromView(target.buffer({2}), 1)), std::invalid_argument);
                vsl::FrameBufferAccessor kept;
                {
                    const std::array<l::ResourceInit, 2> imported{target.frame_buffer({1}), buffer};
                    l::ResourceArena importer(device, import_plan, imported);
                    c = importer.owner();
                    kept = importer.frame_buffer({1});
                    EXPECT_EQ(kept.data_, target.frame_buffer({1}).data_);
                    EXPECT_FALSE(kept.data_.owner_before(target.frame_buffer({1}).data_));
                    EXPECT_FALSE(target.frame_buffer({1}).data_.owner_before(kept.data_));
                    EXPECT_THROW(set.write<0>(View::FromView(importer.buffer({2}), 1)), std::invalid_argument);
                }
                EXPECT_TRUE(c.expired());
                EXPECT_NO_THROW(set.require_complete());
            }
            EXPECT_TRUE(a.expired());
            EXPECT_TRUE(b.expired());
        }
    }
    TEST_F(FrameBufferArenaVulkanIntegrationTest, LaterSlotFailureReleasesEarlierFramesAndAllowsTheSameRegionToBeReused) {
        constexpr std::array<t::ResourceRequest, 2> requests{{{{1}, s::ResourceKind::FRAME_BUFFER}, {{2}, s::ResourceKind::FRAME_BUFFER}}};
        constexpr t::ResourcePlan<2> plan(requests);
        s::ArenaMemory memory(plan.bytes(), plan.alignment());
        ColorImage wrong(device, WIDTH, HEIGHT, vsl::data_format::UNSIGNED_INT);
        auto valid = input();
        valid.attachments.push_back(valid.attachments.front());
        auto invalid = valid;
        invalid.attachments[1][0] = vsl::ImageAccessor{wrong};
        std::array<l::ResourceInit, 2> inputs{valid, invalid};
        const auto pass_count = pass->data_.use_count(), image_count = color->data_.use_count();
        for (int attempt = 0; attempt < 3; ++attempt) {
            EXPECT_THROW((l::ResourceArena(device, plan, inputs, memory)), std::invalid_argument);
            EXPECT_EQ(pass->data_.use_count(), pass_count);
            EXPECT_EQ(color->data_.use_count(), image_count);
        }
        inputs[1] = valid;
        EXPECT_NO_THROW((l::ResourceArena(device, plan, inputs, memory)));
        std::get<l::FrameBufferInit>(inputs[1]).attachments.clear();
        EXPECT_THROW((l::ResourceArena(device, plan, inputs, memory)), std::invalid_argument);
        inputs[1] = valid;
        EXPECT_NO_THROW((l::ResourceArena(device, plan, inputs, memory)));
    }
    TEST_F(FrameBufferArenaVulkanIntegrationTest, NativeBatchFailureDestroysSuccessfulRowsInReverseAndCanBeRetried) {
        for (const size_t fail_at : {size_t(2), size_t(4)}) {
            NativeFailure failure(fail_at);
            const vsl::operation::FrameBufferNativeFactory factory{&failure, NativeFailure::Create, NativeFailure::Destroy};
            vsl::impl::FrameBufferImpl target;
            const vsl::operation::FrameBufferInput definition{pass->data_.get(), WIDTH, HEIGHT, 1, {{color->data_.get()}, {color->data_.get()}, {color->data_.get()}, {color->data_.get()}}};
            EXPECT_THROW(vsl::operation::initialize_frame_buffer(target, *device.data_, definition, factory), std::runtime_error);
            EXPECT_EQ(failure.created.size(), fail_at - 1);
            EXPECT_EQ(failure.destroyed.size(), fail_at - 1);
            EXPECT_TRUE(std::ranges::equal(failure.created | std::views::reverse, failure.destroyed));
            EXPECT_TRUE(std::ranges::all_of(target.swap_chain_framebuffers, [](const auto value) { return !value; }));
            EXPECT_NO_THROW(vsl::operation::initialize_frame_buffer(target, *device.data_, definition));
            EXPECT_TRUE(std::ranges::all_of(target.swap_chain_framebuffers, [](const auto value) { return bool(value); }));
            EXPECT_THROW(vsl::operation::initialize_frame_buffer(target, *device.data_, definition), std::logic_error);
        }
    }
    TEST_F(FrameBufferArenaVulkanIntegrationTest, DirectInputsAndImportsRejectNonowningAccessors) {
        auto invalid = input();
        invalid.pass = vsl::RenderPassAccessor{std::shared_ptr<vsl::impl::RenderPassImpl>(std::shared_ptr<void>{}, pass->data_.get())};
        const std::array<l::ResourceInit, 1> pass_input{invalid};
        EXPECT_THROW((l::ResourceArena(device, PLAN, pass_input)), std::invalid_argument);
        invalid = input();
        invalid.attachments[0][0] = vsl::ImageAccessor{std::shared_ptr<vsl::impl::ImageImpl>(std::shared_ptr<void>{}, color->data_.get())};
        const std::array<l::ResourceInit, 1> image_input{invalid};
        EXPECT_THROW((l::ResourceArena(device, PLAN, image_input)), std::invalid_argument);
        constexpr std::array<t::ResourceRequest, 1> requests{{{{1}, s::ResourceKind::FRAME_BUFFER, true}}};
        constexpr t::ResourcePlan<1> plan(requests);
        const vsl::FrameBufferAccessor borrowed{std::shared_ptr<vsl::impl::FrameBufferImpl>(std::shared_ptr<void>{}, framebuffer->data_.get())};
        const std::array<l::ResourceInit, 1> imported{borrowed};
        EXPECT_THROW((l::ResourceArena(device, plan, imported)), std::invalid_argument);
        EXPECT_THROW((vsl::FrameBuffer(*pass, WIDTH, HEIGHT, {std::get<vsl::ImageAccessor>(invalid.attachments[0][0])})), std::invalid_argument);
    }
    TEST_F(FrameBufferArenaVulkanIntegrationTest, SavedBeginAndTypedDrawKeepTheArenaUntilCompletionAndSlotReplacement) {
        using Draw = t::TypedDraw<t::TypedVertexInput<>, t::ResourceSchema<>, t::PushSchema<>>;
        std::optional<vsl::command::RenderPassBegin> begin;
        std::optional<Draw> draw;
        std::weak_ptr<void> owner;
        {
            const std::array<l::ResourceInit, 1> inputs{input()};
            l::ResourceArena arena(device, PLAN, inputs);
            owner = arena.owner();
            t::TypedGraphicsPipeline<t::TypedVertexInput<>> selected(device, *pass, pl::ShaderGroup("placed framebuffer", {*vertex, *fragment}),
                                                                     pl::ColorBlend{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{}, pl::DynamicState{},
                                                                     vsl::Viewport(description), vsl::Scissor(description));
            begin.emplace(arena.frame_buffer({1}));
            draw.emplace(selected.bind_vertices().draw(3));
        }
        auto manager = make_manager();
        vsl::Completion completion;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << *begin << vsl::Viewport(description) << vsl::Scissor(description) << *draw << vsl::command::RenderPassEnd{};
            begin.reset();
            draw.reset();
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            completion = phase.completion();
            ASSERT_TRUE(completion.wait());
        }
        EXPECT_FALSE(owner.expired());
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
    TEST_F(FrameBufferArenaVulkanIntegrationTest, PollCanDestroyTheBeginAndOriginalFacadeWithoutInvalidatingTheRecordedTarget) {
        auto previous = make_manager(), manager = make_manager();
        std::optional<vsl::command::RenderPassBegin> begin;
        vsl::FrameBufferAccessor original;
        std::weak_ptr<void> owner;
        {
            const std::array<l::ResourceInit, 1> inputs{input()};
            l::ResourceArena arena(device, PLAN, inputs);
            owner = arena.owner();
            original = arena.frame_buffer({1});
            begin.emplace(original, std::vector<vsl::ClearValue>{vsl::ClearColor{1, 0, 0, 1}});
        }
        bool called = false;
        const DeferredAction callback([&] { called = true; begin.reset(); original = {}; });
        ASSERT_NO_FATAL_FAILURE(prime_callback(previous, callback));
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            begin->invoke(vsl::CommandPool{CommandAccess::Pool(CommandAccess::Data(manager)->command_pool)}, manager.get_buffer(), manager);
            EXPECT_TRUE(called);
            EXPECT_FALSE(owner.expired());
            phase << vsl::command::RenderPassEnd{};
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        expect_clear(manager, *color, {255, 0, 0, 255});
    }
    TEST_F(FrameBufferArenaVulkanIntegrationTest, PollCancellationRejectsNativeRecordingAndReleasesTheTargetAfterThePhase) {
        auto previous = make_manager(), manager = make_manager();
        std::weak_ptr<void> owner;
        std::optional<vsl::command::RenderPassBegin> begin;
        vsl::Completion completion;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            completion = phase.completion();
            {
                const std::array<l::ResourceInit, 1> inputs{input()};
                l::ResourceArena arena(device, PLAN, inputs);
                owner = arena.owner();
                begin.emplace(arena.frame_buffer({1}));
            }
            bool called = false;
            const DeferredAction callback([&] { called = true; begin.reset(); phase.cancel(); });
            ASSERT_NO_FATAL_FAILURE(prime_callback(previous, callback));
            EXPECT_THROW(begin->invoke(vsl::CommandPool{CommandAccess::Pool(CommandAccess::Data(manager)->command_pool)}, manager.get_buffer(), manager), std::logic_error);
            EXPECT_TRUE(called);
            EXPECT_FALSE(owner.expired());
        }
        EXPECT_TRUE(owner.expired());
        EXPECT_EQ(completion.status(), vsl::Completion::Status::CANCELLED);
    }
    TEST_F(FrameBufferArenaVulkanIntegrationTest, PollRejectsAnotherBeginOnTheSameBufferAndTheGuardIsReleasedAfterward) {
        auto previous = make_manager(), manager = make_manager();
        vsl::command::RenderPassBegin begin(*framebuffer);
        bool called = false;
        const DeferredAction callback([&] {
            called = true;
            vsl::command::RenderPassBegin nested(*framebuffer);
            EXPECT_THROW(nested.invoke(vsl::CommandPool{CommandAccess::Pool(CommandAccess::Data(manager)->command_pool)}, manager.get_buffer(), manager), std::logic_error);
            EXPECT_THROW(vsl::command::RenderPassEnd{}.invoke(vsl::CommandPool{CommandAccess::Pool(CommandAccess::Data(manager)->command_pool)}, manager.get_buffer(), manager), std::logic_error);
            EXPECT_THROW(vsl::command::NextSubpass{}.invoke(vsl::CommandPool{CommandAccess::Pool(CommandAccess::Data(manager)->command_pool)}, manager.get_buffer(), manager), std::logic_error);
        });
        ASSERT_NO_FATAL_FAILURE(prime_callback(previous, callback));
        auto phase = manager.start_phase<vsl::ComputePhase>();
        begin.invoke(vsl::CommandPool{CommandAccess::Pool(CommandAccess::Data(manager)->command_pool)}, manager.get_buffer(), manager);
        EXPECT_TRUE(called);
        phase << vsl::command::RenderPassEnd{} << vsl::command::RenderPassBegin(*framebuffer) << vsl::command::RenderPassEnd{};
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
    }
    TEST_F(FrameBufferArenaVulkanIntegrationTest, MultipleRowsAndAttachmentsUseThePhysicalImageStateAndClearTypes) {
        ColorImage ids0(device, WIDTH, HEIGHT, vsl::data_format::UNSIGNED_INT);
        ColorImage ids1(device, WIDTH, HEIGHT, vsl::data_format::UNSIGNED_INT);
        vsl::RenderPassDescription description;
        description.add_color({.format = color->get_format(), .final_layout = vsl::ImageLayout::GENERAL});
        description.add_color({.format = ids0.get_format(), .final_layout = vsl::ImageLayout::GENERAL});
        vsl::RenderPass selected(device, description);
        const std::array<l::ResourceInit, 1> inputs{l::FrameBufferInit{vsl::RenderPassAccessor{selected}, WIDTH, HEIGHT, 1, {{vsl::ImageAccessor{*color}, vsl::ImageAccessor{ids0}}, {vsl::ImageAccessor{*color}, vsl::ImageAccessor{ids1}}}}};
        l::ResourceArena arena(device, PLAN, inputs);
        const auto target = arena.frame_buffer({1});
        auto manager = make_manager();
        for (std::uint32_t index = 0; index < 2; ++index) {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << vsl::command::RenderPassBegin(target, {vsl::ClearColor{0, 0, 1, 1}, vsl::ClearUint{71 + index}}, {}, index)
                  << vsl::command::RenderPassEnd{};
            EXPECT_TRUE(color->use_state().busy());
            EXPECT_TRUE((index ? ids1 : ids0).use_state().busy());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        expect_clear(manager, *color, {0, 0, 255, 255});
        for (std::uint32_t index = 0; index < 2; ++index) {
            const auto bytes = vsl_test::read_image(manager, index ? vsl::ImageAccessor{ids1} : vsl::ImageAccessor{ids0}, vsl::ImageLayout::GENERAL);
            ASSERT_EQ(bytes.size(), WIDTH * HEIGHT * sizeof(std::uint32_t));
            for (size_t offset = 0; offset < bytes.size(); offset += sizeof(std::uint32_t)) {
                std::uint32_t pixel;
                std::memcpy(&pixel, bytes.data() + offset, sizeof(pixel));
                EXPECT_EQ(pixel, 71u + index);
            }
        }
    }
    TEST_F(FrameBufferArenaVulkanIntegrationTest, MutableLegacyFieldsAreRevalidatedBeforeNativeRecording) {
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        vsl::command::RenderPassBegin begin(*framebuffer);
        const auto valid = begin.clear_values;
        begin.clear_values.clear();
        EXPECT_THROW(begin.invoke(vsl::CommandPool{CommandAccess::Pool(CommandAccess::Data(manager)->command_pool)}, manager.get_buffer(), manager), std::invalid_argument);
        begin.clear_values = {vsl::ClearUint{}};
        EXPECT_THROW(begin.invoke(vsl::CommandPool{CommandAccess::Pool(CommandAccess::Data(manager)->command_pool)}, manager.get_buffer(), manager), std::invalid_argument);
        begin.clear_values = valid;
        begin.area.width = WIDTH + 1;
        EXPECT_THROW(begin.invoke(vsl::CommandPool{CommandAccess::Pool(CommandAccess::Data(manager)->command_pool)}, manager.get_buffer(), manager), std::out_of_range);
        EXPECT_FALSE(color->use_state().busy());
        begin.area = {};
        phase << begin << vsl::command::RenderPassEnd{};
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
    }
    TEST_F(FrameBufferArenaVulkanIntegrationTest, ReplacementKeepsOldCommandsAndACompatibleTypedPipelineWithoutPinningOldAttachments) {
        constexpr std::array<t::ResourceRequest, 3> requests{{{{1}, s::ResourceKind::FRAME_BUFFER}, {{2}, s::ResourceKind::RENDER_PASS}, {{3}, s::ResourceKind::IMAGE}}};
        constexpr t::ResourcePlan<3> plan(requests);
        const auto make_inputs = [&](std::uint32_t width) {
            return std::array<l::ResourceInit, 3>{l::FrameBufferInit{t::ResourceId{2}, width, HEIGHT, 1, {{t::ResourceId{3}}}}, pass->description(),
                                                  vsl::ImageCreateInfo{vsl::ImageType::COLOR_ATTACHMENT | vsl::ImageType::TRANSFER_SRC, width, HEIGHT, color->get_format()}};
        };
        auto inputs = make_inputs(WIDTH);
        l::ResourceArena current(device, plan, inputs);
        std::weak_ptr<void> old_owner = current.owner();
        std::optional<vsl::ImageAccessor> old_image{current.image({3})};
        std::optional<vsl::command::RenderPassBegin> old_begin{current.frame_buffer({1})};
        t::TypedGraphicsPipeline<t::TypedVertexInput<>> shared(device, current.render_pass({2}), pl::ShaderGroup("resize framebuffer", {*vertex, *fragment}),
                                                               pl::ColorBlend{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{}, pl::DynamicState{},
                                                               vsl::Viewport(description), vsl::Scissor(description));
        const auto native_pipeline = shared.backend().data_->pipeline;
        const auto original = current.frame_buffer({1}).data_.get();
        auto invalid = make_inputs(WIDTH * 2);
        std::get<l::FrameBufferInit>(invalid[0]).width += 1;
        EXPECT_THROW((l::ResourceArena(device, plan, invalid)), std::invalid_argument);
        EXPECT_EQ(current.frame_buffer({1}).data_.get(), original);
        inputs = make_inputs(WIDTH * 2);
        current = l::ResourceArena(device, plan, inputs);
        EXPECT_NE(current.frame_buffer({1}).data_.get(), original);
        EXPECT_FALSE(old_owner.expired());
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << *old_begin << vsl::Viewport(0, 0, WIDTH, HEIGHT) << vsl::Scissor(0, 0, WIDTH, HEIGHT)
                  << shared.bind_vertices().draw(3) << vsl::command::RenderPassEnd{};
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        const auto bytes = vsl_test::read_image(manager, *old_image, vsl::ImageLayout::GENERAL);
        std::array<std::uint8_t, 4> pixel{};
        std::memcpy(pixel.data(), bytes.data() + ((HEIGHT / 2) * WIDTH + WIDTH / 2) * 4, 4);
        ExpectTriangleColor(pixel);
        old_begin.reset();
        old_image.reset();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << vsl::command::RenderPassBegin(current.frame_buffer({1})) << vsl::Viewport(0, 0, WIDTH * 2, HEIGHT)
                  << vsl::Scissor(0, 0, WIDTH * 2, HEIGHT) << shared.bind_vertices().draw(3) << vsl::command::RenderPassEnd{};
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        EXPECT_TRUE(old_owner.expired());
        EXPECT_EQ(shared.backend().data_->pipeline, native_pipeline);
        const auto resized = vsl_test::read_image(manager, current.image({3}), vsl::ImageLayout::GENERAL);
        ASSERT_EQ(resized.size(), WIDTH * 2 * HEIGHT * 4);
        std::memcpy(pixel.data(), resized.data() + ((HEIGHT / 2) * WIDTH * 2 + WIDTH) * 4, 4);
        ExpectTriangleColor(pixel);
    }
} // namespace
