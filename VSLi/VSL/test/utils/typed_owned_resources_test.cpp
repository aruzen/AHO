#include <VSL/lifetime/command_access.hpp>
#include "support/vulkan_fixture.hpp"
#include <VSL/utils/typed_resource_bundle.hpp>
#include <VSL/lifetime/descriptor_bindings.hpp>
#include <VSL/utils/typed_compute.hpp>
#include <VSL/vulkan/commands/image_upload.hpp>
#include <VSL/vulkan/commands/image_readback.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace {
    namespace t = vsl::typed;
    using Type = vsl::graphic_resource::Type;
    constexpr auto COMPUTE = vsl::ShaderFlag::COMPUTE;
    constexpr auto SHARED = t::ResourceScope::SHARED, FRAME = t::ResourceScope::PER_FRAME;
    constexpr auto IMAGE = t::ResourceComponent::IMAGE, SAMPLER = t::ResourceComponent::SAMPLER;
    constexpr auto USAGE = vsl::ImageType::SAMPLED | vsl::ImageType::TRANSFER_DST | vsl::ImageType::TRANSFER_SRC;
    constexpr vsl::ImageCreateInfo IMAGE_INFO{USAGE, 1, 1, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA};
    using Resources = t::ResourceSchema<t::DescriptorSet<0, t::StorageBinding<0, std::uint32_t, COMPUTE>, t::TextureBinding<1, COMPUTE>,
                                                         t::DescriptorBinding<2, Type::SAMPLED_IMAGE, COMPUTE>, t::DescriptorBinding<3, Type::SAMPLER, COMPUTE>>>;
    using Schema = t::ResourceAllocationSchema<Resources, t::BufferAllocation<0, 0, t::FixedElements<2>>, t::TextureAllocation<0, 1>, t::ImageAllocation<0, 2>, t::SamplerAllocation<0, 3>>;
    using Map = t::ResourceBindingMap<t::ResourceBinding<t::BindingKey{0, 1}, t::ResourceId{41}, IMAGE>, t::ResourceBinding<t::BindingKey{0, 2}, t::ResourceId{41}>,
                                      t::ResourceBinding<t::BindingKey{0, 1}, t::ResourceId{42}, SAMPLER>, t::ResourceBinding<t::BindingKey{0, 3}, t::ResourceId{42}>>;
    using Arena = t::ArenaStorage<Map>;
    using Bundle = t::TypedResourceBundle<Schema, Arena>;
    class FailingDescriptorManager : public vsl::GraphicResourceManager {
        std::weak_ptr<vsl::impl::GraphicResourceImpl> _previous;

    public:
        using vsl::GraphicResourceManager::GraphicResourceManager;
        bool fail = true;
        std::vector<std::weak_ptr<void>> observed;
        std::tuple<std::vector<vsl::graphic_resource::Pool>, std::vector<vsl::graphic_resource::Resource>> allocate(std::vector<vsl::graphic_resource::BindingLayout> layouts) override;
    };
    t::ResourceAllocationSizes<Schema> sizes();
    template<typename A, typename B>
    bool same_owner(const A& a, const B& b);
    class TypedOwnedResourcesVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }

    protected:
        template<typename Policy>
        void compute_round_trip();
        void host_visible(vsl::CommandManager manager);
    };

    template<typename A, typename B>
    bool same_owner(const A& a, const B& b) {
        return !a.owner_before(b) && !b.owner_before(a);
    }
    t::ResourceAllocationSizes<Schema> sizes() {
        t::ResourceAllocationSizes<Schema> result;
        result.image<0, 1>(IMAGE_INFO).image<0, 2>(IMAGE_INFO);
        return result;
    }
    auto FailingDescriptorManager::allocate(std::vector<vsl::graphic_resource::BindingLayout> layouts) -> std::tuple<std::vector<vsl::graphic_resource::Pool>, std::vector<vsl::graphic_resource::Resource>> {
        if (auto previous = _previous.lock(); fail && previous) {
            for (const auto& owner : previous->owner_context->dependencies) {
                const auto shared = owner.share();
                const auto matches = [&](const auto* node) { return node && node->matches(shared); };
                if (!matches(previous->pool->owner_node) && !matches(previous->allocation_layout->owner_node))
                    observed.push_back(shared);
            }
            for (const auto& [binding, elements] : previous->bindings)
                for (const auto& element : elements)
                    for (const auto& owner : element.owners)
                        observed.push_back(owner.share());
            throw std::runtime_error("Injected descriptor failure after the first mixed frame");
        }
        auto result = vsl::GraphicResourceManager::allocate(std::move(layouts));
        _previous = std::get<1>(result).front().data_;
        return result;
    }
    void TypedOwnedResourcesVulkanIntegrationTest::host_visible(vsl::CommandManager manager) {
        const vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
        auto buffer = manager.get_buffer();
        CommandAccess::Data(buffer)->command_buffers.at(buffer.get_current_buffer_idx()).pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
    }
    template<typename Policy>
    void TypedOwnedResourcesVulkanIntegrationTest::compute_round_trip() {
        auto manager = make_manager().independent(2);
        vsl::GraphicResourceManager descriptors(device);
        t::TypedResourceBundle<Schema, Policy> bundle(manager, descriptors, sizes());
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_arena_images.comp.spv"));
        t::TypedComputePipeline<Resources> pipeline(device, vsl::pipeline_layout::ShaderGroup("owned images", {shader}));
        const std::array<std::uint8_t, 4> color{37, 83, 0, 255};
        vsl::StagingBuffer<> pixels(device, color);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> readback(device, color.size());
        std::array<std::shared_ptr<vsl::impl::ImageImpl>, 2> generations;
        std::shared_ptr<vsl::impl::SamplerImpl> shared_sampler;
        for (size_t i = 0; i < 2; ++i) {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            EXPECT_EQ(phase.frame_slot, i);
            bundle.update(phase, [&](auto& frame) {
                phase << vsl::command::ImageUpload(&pixels, frame.template image<0, 1>(), vsl::ImageLayout::UNDEFINED);
                if constexpr (!t::detail::BundlePolicy<Policy>::ARENA)
                    phase << vsl::command::ImageUpload(&pixels, frame.template image<0, 2>(), vsl::ImageLayout::UNDEFINED);
            });
            const auto& frame = bundle.get(phase);
            generations[i] = frame.template image<0, 1>().data_;
            if (!shared_sampler)
                shared_sampler = frame.template sampler<0, 1>().data_;
            EXPECT_EQ(shared_sampler, (frame.template sampler<0, 1>().data_));
            if constexpr (t::detail::BundlePolicy<Policy>::ARENA) {
                EXPECT_EQ((frame.template image<0, 1>().data_), (frame.template image<0, 2>().data_));
                EXPECT_EQ(shared_sampler, (frame.template sampler<0, 3>().data_));
                EXPECT_TRUE(same_owner(frame.template buffer<0, 0>().storage_view().owner(), generations[i]));
                EXPECT_FALSE(same_owner(shared_sampler, generations[i]));
            }
            phase << frame.bind_resources(pipeline).dispatch(1);
            phase << vsl::command::ImageReadback(&readback, frame.template image<0, 1>(), vsl::ImageLayout::SHADER_READ_ONLY_OPTIMAL);
            EXPECT_TRUE((frame.template image<0, 1>().use_state().busy()));
            host_visible(manager);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
            std::array<std::uint32_t, 2> actual{};
            if constexpr (t::detail::BundlePolicy<Policy>::ARENA)
                frame.template buffer<0, 0>().read_into(actual);
            else
                frame.template buffer<0, 0>().storage_view().read_into(std::as_writable_bytes(std::span(actual)));
            EXPECT_EQ(actual, (std::array<std::uint32_t, 2>{37, 83}));
            std::array<std::uint8_t, 4> texel{};
            readback.read_into(std::as_writable_bytes(std::span(texel)));
            EXPECT_EQ(texel, color);
            manager.get_buffer().next();
        }
        EXPECT_NE(generations[0], generations[1]);
    }
    TEST_F(TypedOwnedResourcesVulkanIntegrationTest, ArenaComponentsSharePhysicalStorageAcrossBindingsAndComputeCorrectly) {
        compute_round_trip<Arena>();
    }
    TEST_F(TypedOwnedResourcesVulkanIntegrationTest, HeapPolicyUsesTheSameImageAndSamplerBackend) {
        compute_round_trip<t::HeapStorage>();
    }
    TEST_F(TypedOwnedResourcesVulkanIntegrationTest, SharedSamplerViewDoesNotKeepFrameImagesAlive) {
        auto manager = make_manager().independent(2);
        vsl::GraphicResourceManager descriptors(device);
        std::optional<vsl::SamplerAccessor> retained;
        std::weak_ptr<void> shared, frame_owner;
        {
            Bundle bundle(manager, descriptors, sizes());
            bundle.initialize_shared([&](auto& values) { retained = values.template sampler<0, 1>(); });
            auto phase = manager.start_phase<vsl::ComputePhase>();
            const auto& frame = bundle.get(phase);
            shared = retained->data_;
            frame_owner = frame.image<0, 1>().data_;
            EXPECT_FALSE(same_owner(shared, frame_owner));
            phase.cancel();
        }
        EXPECT_TRUE(frame_owner.expired());
        EXPECT_FALSE(shared.expired());
        retained.reset();
        EXPECT_TRUE(shared.expired());
    }
    TEST_F(TypedOwnedResourcesVulkanIntegrationTest, ImageArraysKeepDistinctExtentsAndStorageInOneLifetimeGroup) {
        using Arrays = t::ResourceAllocationSchema<t::ResourceSchema<t::DescriptorSet<0, t::TextureBinding<0, COMPUTE, 2>>>, t::TextureAllocation<0, 0>>;
        t::ResourceAllocationSizes<Arrays> infos;
        auto larger = IMAGE_INFO;
        larger.width = 3;
        larger.height = 2;
        infos.image<0, 0>(std::array{IMAGE_INFO, larger});
        auto manager = make_manager();
        vsl::GraphicResourceManager descriptors(device);
        t::TypedResourceBundle<Arrays, t::ArenaStorage<>> bundle(manager, descriptors, infos);
        auto phase = manager.start_phase<vsl::ComputePhase>();
        const auto& frame = bundle.get(phase);
        const auto first = frame.image<0, 0, 0>(), second = frame.image<0, 0, 1>();
        EXPECT_EQ(first.data_->width, 1u);
        EXPECT_EQ(second.data_->width, 3u);
        EXPECT_EQ(second.data_->height, 2u);
        EXPECT_NE(first.data_, second.data_);
        EXPECT_TRUE(same_owner(first.data_, second.data_));
        EXPECT_NE(&first.use_state(), &second.use_state());
        EXPECT_FALSE(same_owner(first.data_, frame.sampler<0, 0, 0>().data_));
        EXPECT_NO_THROW(frame.set<0>().require_complete());
        phase.cancel();
    }
    TEST_F(TypedOwnedResourcesVulkanIntegrationTest, SharedImageAndFrameSamplerAreComposedOnlyInTheFrame) {
        using Reverse = t::ResourceAllocationSchema<t::ResourceSchema<t::DescriptorSet<0, t::TextureBinding<0, COMPUTE>>>, t::TextureAllocation<0, 0, SHARED, FRAME>>;
        t::ResourceAllocationSizes<Reverse> infos;
        infos.image<0, 0>(IMAGE_INFO);
        auto manager = make_manager().independent(2);
        vsl::GraphicResourceManager descriptors(device);
        std::optional<vsl::ImageAccessor> retained;
        std::weak_ptr<void> frame_owner;
        {
            t::TypedResourceBundle<Reverse, t::ArenaStorage<>> bundle(manager, descriptors, infos);
            bundle.initialize_shared([&](auto& shared) { retained = shared.template image<0, 0>(); });
            auto phase = manager.start_phase<vsl::ComputePhase>();
            const auto& frame = bundle.get(phase);
            EXPECT_EQ(retained->data_, (frame.image<0, 0>().data_));
            frame_owner = frame.sampler<0, 0>().data_;
            EXPECT_FALSE(same_owner(retained->data_, frame_owner));
            phase.cancel();
        }
        EXPECT_TRUE(frame_owner.expired());
        EXPECT_EQ(retained->data_->width, 1u);
    }
    TEST_F(TypedOwnedResourcesVulkanIntegrationTest, DeferredCommandRetainsOwnersUntilCancellationAfterBundleDestruction) {
        auto manager = make_manager();
        vsl::GraphicResourceManager descriptors(device);
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_arena_images.comp.spv"));
        t::TypedComputePipeline<Resources> pipeline(device, vsl::pipeline_layout::ShaderGroup("owned cancellation", {shader}));
        std::weak_ptr<void> frame_owner, shared_owner;
        auto make_command = [&] {
            auto source = manager.start_phase<vsl::ComputePhase>();
            Bundle bundle(manager, descriptors, sizes());
            const auto& frame = bundle.get(source);
            frame_owner = frame.image<0, 1>().data_;
            shared_owner = frame.sampler<0, 1>().data_;
            auto command = frame.bind_resources(pipeline).dispatch(1);
            source.cancel();
            return command;
        };
        std::optional command(make_command());
        EXPECT_FALSE(frame_owner.expired());
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << *command;
        command.reset();
        EXPECT_FALSE(frame_owner.expired());
        EXPECT_FALSE(shared_owner.expired());
        phase.cancel();
        EXPECT_TRUE(frame_owner.expired());
        EXPECT_TRUE(shared_owner.expired());
    }
    TEST_F(TypedOwnedResourcesVulkanIntegrationTest, SharedInitializationRejectsAnUnfinishedUploadAndPoisonsTheBundle) {
        using SharedImage = t::ResourceAllocationSchema<t::ResourceSchema<t::DescriptorSet<0, t::DescriptorBinding<0, Type::SAMPLED_IMAGE, COMPUTE>>>, t::ImageAllocation<0, 0, SHARED>>;
        t::ResourceAllocationSizes<SharedImage> infos;
        infos.image<0, 0>(IMAGE_INFO);
        auto manager = make_manager();
        vsl::GraphicResourceManager descriptors(device);
        t::TypedResourceBundle<SharedImage, t::ArenaStorage<>> bundle(manager, descriptors, infos);
        const std::array<std::uint8_t, 4> color{37, 83, 0, 255};
        vsl::StagingBuffer<> pixels(device, color);
        auto phase = manager.start_phase<vsl::ComputePhase>();
        EXPECT_THROW(bundle.initialize_shared([&](auto& shared) { phase << vsl::command::ImageUpload(&pixels, shared.template image<0, 0>(), vsl::ImageLayout::UNDEFINED); }), std::logic_error);
        phase.cancel();
        auto fresh = manager.start_phase<vsl::ComputePhase>();
        EXPECT_THROW(bundle.get(fresh), std::logic_error);
        EXPECT_THROW(bundle.update(fresh, [](auto&) {}), std::logic_error);
        fresh.cancel();
    }
    TEST_F(TypedOwnedResourcesVulkanIntegrationTest, CompletedSharedUploadAllowsFrameUseAndReleasesOwnersAfterGpuCompletion) {
        using SharedImage = t::ResourceAllocationSchema<t::ResourceSchema<t::DescriptorSet<0, t::TextureBinding<0, COMPUTE>>>, t::TextureAllocation<0, 0, SHARED, SHARED>>;
        t::ResourceAllocationSizes<SharedImage> infos;
        infos.image<0, 0>(IMAGE_INFO);
        auto manager = make_manager();
        vsl::GraphicResourceManager descriptors(device);
        const std::array<std::uint8_t, 4> color{37, 83, 0, 255};
        vsl::StagingBuffer<> pixels(device, color);
        std::weak_ptr<void> owner;
        auto phase = manager.start_phase<vsl::ComputePhase>();
        const auto completion = phase.completion();
        {
            t::TypedResourceBundle<SharedImage, t::ArenaStorage<>> bundle(manager, descriptors, infos);
            bundle.initialize_shared([&](auto& shared) {
                auto upload = make_manager();
                auto initialization = upload.start_phase<vsl::ComputePhase>();
                initialization << vsl::command::ImageUpload(&pixels, shared.template image<0, 0>(), vsl::ImageLayout::UNDEFINED);
                if (initialization.submit() != vsl::ComputePhase::SubmitResult::SUCCESS || !initialization.completion().wait())
                    throw std::runtime_error("Shared initialization failed");
            });
            const auto& frame = bundle.get(phase);
            owner = frame.image<0, 0>().data_;
            frame.set<0>().resource_use()->record(manager.get_buffer());
            EXPECT_TRUE((frame.image<0, 0>().use_state().busy()));
        }
        EXPECT_FALSE(owner.expired());
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(completion.wait());
        EXPECT_TRUE(owner.expired());
    }
    TEST_F(TypedOwnedResourcesVulkanIntegrationTest, ExternalTexturePreservesItsAdditionalLogicalUpdateConstraint) {
        using External = t::ResourceAllocationSchema<t::ResourceSchema<t::DescriptorSet<0, t::TextureBinding<0, COMPUTE>>>, t::ExternalAllocation<0, 0>>;
        auto manager = make_manager();
        vsl::GraphicResourceManager descriptors(device);
        vsl::Image<USAGE> image(device, 1, 1, IMAGE_INFO.format);
        vsl::Sampler sampler(device);
        auto logical = std::make_shared<vsl::ResourceUseState>();
        auto logical_owner = std::make_shared<int>(7);
        const t::TypedTexture texture(image, sampler, image.resource_use().with_constraint({logical, logical_owner}));
        t::TypedResourceBundle<External, t::ArenaStorage<>> bundle(manager, descriptors, t::external_resource<0, 0>(texture));
        auto phase = manager.start_phase<vsl::ComputePhase>();
        bundle.get(phase).set<0>().resource_use()->record(manager.get_buffer());
        EXPECT_TRUE(logical->busy());
        EXPECT_TRUE(image.use_state().busy());
        EXPECT_TRUE(sampler.use_state().busy());
        phase.cancel();
        EXPECT_FALSE(logical->busy());
        EXPECT_FALSE(image.use_state().busy());
    }
    TEST_F(TypedOwnedResourcesVulkanIntegrationTest, DescriptorManagerFailureRollsBackMixedHeapOwnersAndAllowsRetry) {
        auto manager = make_manager().independent(2);
        auto verify = [&]<typename Policy>() {
            FailingDescriptorManager descriptors(device);
            EXPECT_THROW((t::TypedResourceBundle<Schema, Policy>(manager, descriptors, sizes())), std::runtime_error);
            EXPECT_FALSE(descriptors.observed.empty());
            for (const auto& owner : descriptors.observed)
                EXPECT_TRUE(owner.expired());
            descriptors.fail = false;
            EXPECT_NO_THROW((t::TypedResourceBundle<Schema, Policy>(manager, descriptors, sizes())));
        };
        verify.template operator()<t::HeapStorage>();
    }
} // namespace
