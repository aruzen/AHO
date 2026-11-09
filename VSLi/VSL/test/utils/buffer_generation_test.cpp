#include "support/vulkan_fixture.hpp"
#include <VSL/utils/typed_buffer_arena.hpp>
#include <VSL/utils/typed_vertex_buffer.hpp>
#include <VSL/utils/typed_index_buffer.hpp>
#include <VSL/utils/typed_resources.hpp>
#include <VSL/vulkan/commands/bind_vertex_buffers.hpp>
#include <VSL/vulkan/commands/bind_index_buffers.hpp>

namespace {
    namespace t = vsl::typed;
    constexpr auto USAGE = vsl::MemoryType::TRANSFER_SOURCE | vsl::MemoryType::TRANSFER_DESTINATION;
    constexpr t::ResourceId SOURCE{10}, DESTINATION{20};
    using Arena = t::TypedBufferArena<t::BufferResource<SOURCE, std::uint32_t, USAGE>,
                                      t::BufferResource<DESTINATION, std::uint32_t, USAGE>>;
    using Native = vsl::Buffer<USAGE, vsl::MemoryProperty::HOST_VISIBLE>;
    const std::array<std::uint32_t, 4> VALUES{1, 2, 3, 4};

    class BufferGenerationVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };

    TEST_F(BufferGenerationVulkanIntegrationTest, VertexAndIndexBindsRetainTheSameOwnerOnlyOnce) {
        using Buffer = vsl::Buffer<vsl::MemoryType::VERTEX_BUFFER | vsl::MemoryType::INDEX_BUFFER,
                                   vsl::MemoryProperty::HOST_VISIBLE>;
        auto manager = make_manager();
        std::weak_ptr<vsl::impl::BufferImpl> observed;
        std::optional<vsl::BufferIdentity> identity;
        vsl::Completion cancelled;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            cancelled = phase.completion();
            {
                Buffer native(device, 16);
                observed = native.data_;
                identity = native.identity();
                EXPECT_EQ(*identity, vsl::BufferView::FromBuffer(native).identity());
                EXPECT_EQ(*identity, vsl::command::BindVertexBufferAt(0, native).identity());
                EXPECT_EQ(*identity, vsl::command::BindIndexBufferAt(native, vsl::command::IndexType::UINT32).identity());
                phase << vsl::command::BindVertexBuffer(native)
                      << vsl::command::BindVertexBufferAt(0, native)
                      << vsl::command::BindIndexBuffer(native)
                      << vsl::command::BindIndexBufferAt(native, vsl::command::IndexType::UINT32);
            }
            EXPECT_EQ(cancelled.data_->work.lock()->resources.size(), 1u);
            EXPECT_FALSE(observed.expired());
            phase.cancel();
        }
        EXPECT_EQ(cancelled.status(), vsl::Completion::Status::CANCELLED);
        EXPECT_TRUE(observed.expired());
        identity.reset();
    }

    TEST_F(BufferGenerationVulkanIntegrationTest, OneArenaOwnerStillRegistersEveryDistinctBufferState) {
        auto manager = make_manager();
        Arena arena(device, Arena::Plan({4, 4}));
        const auto first = arena.view<SOURCE>().backend().identity();
        const auto second = arena.view<DESTINATION>().backend().identity();
        EXPECT_NE(first, second);
        EXPECT_EQ(first, arena.storage().borrow(0).identity());
        auto phase = manager.start_phase<vsl::ComputePhase>();
        arena.view<SOURCE>().backend().resource_use().record(manager.get_buffer());
        arena.view<DESTINATION>().backend().resource_use().record(manager.get_buffer());
        EXPECT_EQ(phase.completion().data_->work.lock()->resources.size(), 1u);
        EXPECT_TRUE(arena.storage().borrow(0).use().busy());
        EXPECT_TRUE(arena.storage().borrow(1).use().busy());
        phase.cancel();
        EXPECT_FALSE(arena.storage().borrow(0).use().busy());
        EXPECT_FALSE(arena.storage().borrow(1).use().busy());
    }

    TEST_F(BufferGenerationVulkanIntegrationTest, DescriptorTrackingReadsCurrentNativeBindingsAndRetainsEachOwnerOnce) {
        using Set = t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>,
                                     t::UniformBinding<1, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
        vsl::GraphicResourceManager descriptors(device);
        t::TypedDescriptorSet<Set> set(device, descriptors);
        t::TypedUniformBuffer<std::uint32_t> first(device, 1u), second(device, 2u);
        set.write<0>(first);
        set.write<1>(second);
        auto native = set.backend();
        native.update(second.backend(), 0);
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        vsl::command::BindGraphicResource command(native, vsl::graphic_resource::BindingDestination::COMPUTE,
                                                  std::nullopt, 0, set.resource_use());
        command.track(manager.get_buffer());
        command.track(manager.get_buffer());
        EXPECT_EQ(phase.completion().data_->work.lock()->completion_resources.size(), 2u);
        EXPECT_FALSE(first.backend().ref().use().busy());
        EXPECT_TRUE(second.backend().ref().use().busy());
        EXPECT_THROW(set.write<0>(first), std::logic_error);
        phase.cancel();
        EXPECT_NO_THROW(set.write<0>(first));
    }

    TEST_F(BufferGenerationVulkanIntegrationTest, OneTypedTrackingGroupStillKeepsEverySetInANativeMultiSetBinding) {
        using Set = t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
        vsl::GraphicResourceManager descriptors(device);
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        std::weak_ptr<vsl::impl::GraphicResourceImpl> first_set, second_set;
        std::weak_ptr<vsl::ResourceUseState> first_buffer, second_buffer;
        {
            t::TypedDescriptorSet<Set> first(device, descriptors), second(device, descriptors);
            t::TypedUniformBuffer<std::uint32_t> a(device, 1u), b(device, 2u);
            first.write<0>(a);
            second.write<0>(b);
            first_set = first.backend().data_;
            second_set = second.backend().data_;
            first_buffer = a.descriptor().resource_use().state.share();
            second_buffer = b.descriptor().resource_use().state.share();
            vsl::command::BindGraphicResource command(std::vector<vsl::GraphicResource>{first.backend(), second.backend()},
                                                      vsl::graphic_resource::BindingDestination::COMPUTE,
                                                      std::nullopt, 0, first.resource_use());
            command.track(manager.get_buffer());
        }
        EXPECT_EQ(phase.completion().data_->work.lock()->resources.size(), 2u);
        EXPECT_EQ(phase.completion().data_->work.lock()->completion_resources.size(), 2u);
        ASSERT_FALSE(first_set.expired());
        ASSERT_FALSE(second_set.expired());
        ASSERT_FALSE(first_buffer.expired());
        ASSERT_FALSE(second_buffer.expired());
        EXPECT_TRUE(first_buffer.lock()->busy());
        EXPECT_TRUE(second_buffer.lock()->busy());
        phase.cancel();
        EXPECT_TRUE(first_set.expired());
        EXPECT_TRUE(first_buffer.expired());
        EXPECT_FALSE(second_set.expired());
        EXPECT_FALSE(second_buffer.expired());
    }

    TEST_F(BufferGenerationVulkanIntegrationTest, NativeTypedAndImportedViewsShareOneStorageState) {
        t::TypedVertexBuffer<std::uint32_t> vertices(device, VALUES);
        t::TypedIndexBuffer<std::uint32_t> indices(device, VALUES);
        t::TypedUniformBuffer<std::uint32_t> uniform(device, VALUES);
        t::TypedStorageBuffer<std::uint32_t> storage(device, VALUES);
        EXPECT_EQ(vertices.resource_use().state.get(), &vertices.backend().ref().use());
        EXPECT_EQ(vertices.view(0, 4).resource_use().state.get(), &vertices.backend().ref().use());
        EXPECT_EQ(indices.resource_use().state.get(), &indices.backend().ref().use());
        EXPECT_EQ(uniform.descriptor().resource_use().state.get(), &uniform.backend().ref().use());
        EXPECT_EQ(storage.descriptor().resource_use().state.get(), &storage.backend().ref().use());
        EXPECT_EQ(vsl::ResourceUse::ForBuffer(uniform.backend()).state.get(), &uniform.backend().ref().use());
        const auto subview = uniform.storage_view().view(sizeof(std::uint32_t), sizeof(std::uint32_t));
        EXPECT_EQ(vsl::ResourceUse::ForBuffer(subview).state.get(), &uniform.backend().ref().use());
        auto imported = t::TypedVertexBufferView<std::uint32_t>::FromView(vertices.backend(), 4);
        EXPECT_EQ(imported.resource_use().state.get(), &vertices.backend().ref().use());
        auto resource = t::TypedResourceBufferView<std::uint32_t, vsl::graphic_resource::Type::UNIFORM_BUFFER>::FromView(uniform.backend(), 4);
        EXPECT_EQ(resource.descriptor().resource_use().state.get(), &uniform.backend().ref().use());
    }

    TEST_F(BufferGenerationVulkanIntegrationTest, NativeCopyBlocksTypedUpdatesAndTypedBindingBlocksNativeUpdates) {
        auto manager = make_manager();
        t::TypedVertexBuffer<std::uint32_t> vertices(device, VALUES);
        Native source(device, sizeof(VALUES));
        source.copy(VALUES);
        auto native = source;
        native.data_ = vertices.storage_view().legacy_owner();
        EXPECT_EQ(vsl::ResourceUse::ForBuffer(native).state.get(), &vertices.backend().ref().use());
        auto mapping = native.data();
        auto view = vertices.storage_view();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << vsl::command::CopyBuffer(&source, &native, sizeof(VALUES));
            EXPECT_THROW(vertices.copy(VALUES), std::logic_error);
            EXPECT_THROW(native.copy(VALUES), std::logic_error);
            EXPECT_THROW(mapping.copy(VALUES), std::logic_error);
            EXPECT_THROW(view.write(std::as_bytes(std::span(VALUES))), std::logic_error);
            phase.cancel();
        }
        EXPECT_NO_THROW(vertices.copy(VALUES));
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << vsl::command::BindVertexBufferAt(0, vertices.view(0, 4).storage_view(), 0, vertices.resource_use());
            std::array<std::byte, sizeof(VALUES)> output{};
            EXPECT_THROW(view.read_into(output), std::logic_error);
            EXPECT_THROW(native.data(), std::logic_error);
            phase.cancel();
        }
        EXPECT_NO_THROW(view.write(std::as_bytes(std::span(VALUES))));
    }

    TEST_F(BufferGenerationVulkanIntegrationTest, ReadIntoFinishesBeforeLaterGpuWritesAndDoesNotLeakAMappedSpan) {
        auto manager = make_manager();
        Arena arena(device, Arena::Plan({4, 4}));
        auto source = arena.view<SOURCE>();
        auto destination = arena.view<DESTINATION>();
        destination.copy(VALUES);
        std::array<std::uint32_t, 4> snapshot{};
        destination.read_into(snapshot);
        const std::array<std::uint32_t, 4> updated{11, 12, 13, 14};
        source.copy(updated);
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << source.copy_to(destination);
        EXPECT_EQ(snapshot, VALUES);
        EXPECT_THROW(destination.read_into(snapshot), std::logic_error);
        EXPECT_EQ(snapshot, VALUES);
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        EXPECT_EQ(snapshot, VALUES);
        destination.read_into(snapshot);
        EXPECT_EQ(snapshot, updated);
        EXPECT_THROW(destination.read_into(std::span(snapshot).first(1)), std::invalid_argument);
        EXPECT_THROW(destination.backend().read_into(std::as_writable_bytes(std::span(snapshot)), 1), std::out_of_range);
    }

    TEST_F(BufferGenerationVulkanIntegrationTest, BindingKeysAliasResourceIdsAndPlanAssignsTheirSlots) {
        using Map = t::BufferBindingMap<t::ResourceBinding<t::BindingKey{2, 0}, SOURCE>,
                                        t::ResourceBinding<t::BindingKey{7, 3, 2}, SOURCE>,
                                        t::ResourceBinding<t::BindingKey{2, 1}, DESTINATION>>;
        static_assert(Map::Resolve<t::BindingKey{7, 3, 2}>() == SOURCE);
        static_assert(Arena::Plan::PLACEMENTS[1].resource == DESTINATION);
        static_assert(Arena::Plan::PLACEMENTS[1].slot == 1);
        Arena arena(device, Arena::Plan({4, 4}));
        auto first = arena.binding_view<Map, t::BindingKey{2, 0}>();
        auto alias = arena.binding_view<Map, t::BindingKey{7, 3, 2}>();
        EXPECT_EQ(first.backend().ref().data, alias.backend().ref().data);
        EXPECT_NE(first.backend().ref().data, arena.view<DESTINATION>().backend().ref().data);
        first.copy(VALUES);
        std::array<std::uint32_t, 4> actual{};
        alias.read_into(actual);
        EXPECT_EQ(actual, VALUES);
    }

    TEST_F(BufferGenerationVulkanIntegrationTest, SharedViewsDoNotKeepUnrelatedFrameArenasAlive) {
        using Shared = t::TypedBufferArena<t::BufferResource<SOURCE, std::uint32_t, USAGE>>;
        using Frame = t::TypedBufferArena<t::BufferResource<DESTINATION, std::uint32_t, USAGE, t::LifetimeScope::FRAME>>;
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        Native output(device, sizeof(VALUES));
        std::optional<t::PlacedBufferView<std::uint32_t, USAGE>> shared_view;
        std::weak_ptr<void> shared_owner, frame_owner, unused_owner;
        {
            Shared shared(device, Shared::Plan({4}));
            Frame frame(device, Frame::Plan({4}, 0)), unused(device, Frame::Plan({4}, 1));
            EXPECT_EQ(frame.lifetime(), (t::LifetimeGroup{t::LifetimeScope::FRAME, 0}));
            EXPECT_EQ(unused.lifetime(), (t::LifetimeGroup{t::LifetimeScope::FRAME, 1}));
            shared_owner = shared.storage().owner();
            frame_owner = frame.storage().owner();
            unused_owner = unused.storage().owner();
            shared_view.emplace(shared.view<SOURCE>());
            shared_view->copy(VALUES);
            frame.view<DESTINATION>().copy(VALUES);
            phase << shared_view->copy_to(frame.view<DESTINATION>());
            phase << frame.view<DESTINATION>().copy_to(t::PlacedBufferView<std::uint32_t, USAGE>::FromBuffer(output));
        }
        EXPECT_TRUE(unused_owner.expired());
        EXPECT_FALSE(frame_owner.expired());
        EXPECT_FALSE(shared_owner.expired());
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        EXPECT_TRUE(frame_owner.expired());
        EXPECT_FALSE(shared_owner.expired());
        std::array<std::uint32_t, 4> actual{};
        shared_view->read_into(actual);
        EXPECT_EQ(actual, VALUES);
        output.read_into(std::as_writable_bytes(std::span(actual)));
        EXPECT_EQ(actual, VALUES);
        shared_view.reset();
        EXPECT_TRUE(shared_owner.expired());
        EXPECT_THROW(Shared::Plan((std::array<size_t, 1>{4}), 1), std::invalid_argument);
    }
} // namespace
