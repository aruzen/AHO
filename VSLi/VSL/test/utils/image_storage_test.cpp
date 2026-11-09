#include <VSL/lifetime/image_access.hpp>
#include <VSL/lifetime/sampler_access.hpp>
#include <VSL/lifetime/command_access.hpp>
#include "support/vulkan_fixture.hpp"
#include <VSL/lifetime/image_storage.hpp>
#include <VSL/lifetime/sampler_storage.hpp>
#include <VSL/lifetime/resource_arena.hpp>
#include <VSL/utils/typed_compute.hpp>
#include <VSL/vulkan/commands/image_upload.hpp>
#include <VSL/vulkan/commands/render_pass_begin.hpp>
#include <VSL/vulkan/commands/render_pass_end.hpp>
#include <VSL/vulkan/commands/image_readback.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::detail::ImageAccess;
    using VSL_NAMESPACE::lifetime::detail::SamplerAccess;
} // namespace

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace {
    namespace t = vsl::typed;
    constexpr auto IMAGE_USAGE = vsl::ImageType::SAMPLED | vsl::ImageType::TRANSFER_DST;
    const vsl::ImageCreateInfo IMAGE_INFO{IMAGE_USAGE, 2, 2, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA};
    const vsl::SamplerCreateInfo SAMPLER_INFO{vsl::SamplingMode::NEAREST, vsl::EdgeMode::CLAMP_TO_EDGE};
    class ImageStorageVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };

    TEST_F(ImageStorageVulkanIntegrationTest, TransferOnlyImagesSkipViewsAndSupportHeapArenaImportAndReadback) {
        const vsl::ImageCreateInfo info{vsl::ImageType::TRANSFER_SRC | vsl::ImageType::TRANSFER_DST, 1, 1, vsl::data_format::UNSIGNED_INT};
        const std::array<std::uint32_t, 1> expected{0x10203040};
        for (const bool arena : {false, true}) {
            constexpr std::array<t::ResourceRequest, 1> requests{{{{1}, vsl::storage::ResourceKind::IMAGE}}};
            constexpr t::ResourcePlan<1> plan(requests);
            const auto image = [&] {
                if (arena) {
                    const std::array<vsl::lifetime::ResourceInit, 1> initializers{info};
                    vsl::lifetime::ResourceArena storage(device, plan, initializers);
                    return storage.image({1});
                }
                return vsl::ImageAccessor::Create(device, info);
            }();
            EXPECT_FALSE(ImageAccess::Owned(image)->view);
            EXPECT_TRUE(image.resource_use().owner.managed());
            constexpr std::array<t::ResourceRequest, 1> external_requests{{{{2}, vsl::storage::ResourceKind::IMAGE, true}}};
            constexpr t::ResourcePlan<1> external_plan(external_requests);
            const std::array<vsl::lifetime::ResourceInit, 1> external_inputs{image};
            vsl::lifetime::ResourceArena external(device, external_plan, external_inputs);
            const auto imported = external.image({2});
            EXPECT_EQ(&imported.use_state(), &image.use_state());
            EXPECT_FALSE(ImageAccess::Owned(imported)->view);
            vsl::StagingBuffer<> source(device, expected);
            vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> destination(device, std::size_t{4});
            auto manager = make_manager();
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << vsl::command::ChangeImageBarrier(imported, vsl::ImageLayout::UNDEFINED, vsl::ImageLayout::TRANSFER_DST_OPTIMAL)
                  << vsl::command::CopyBufferToImage(imported, &source, vsl::ImageLayout::TRANSFER_DST_OPTIMAL)
                  << vsl::command::ImageReadback(&destination, imported, vsl::ImageLayout::TRANSFER_DST_OPTIMAL);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
            std::array<std::uint32_t, 1> actual{};
            destination.read_into(std::as_writable_bytes(std::span(actual)));
            EXPECT_EQ(actual, expected);
        }
    }

    TEST_F(ImageStorageVulkanIntegrationTest, PlacementConstructsDistinctImagesAndSamplerWithTheirOwnUseStates) {
        std::array<vsl::lifetime::ImageSlotStorage, 2> images;
        vsl::lifetime::SamplerSlotStorage sampler;
        vsl::lifetime::construct_image(&images[0], device, IMAGE_INFO);
        const auto first = std::unique_ptr<void, decltype(&vsl::lifetime::destroy_image)>(&images[0], vsl::lifetime::destroy_image);
        vsl::lifetime::construct_image(&images[1], device, IMAGE_INFO);
        const auto second = std::unique_ptr<void, decltype(&vsl::lifetime::destroy_image)>(&images[1], vsl::lifetime::destroy_image);
        vsl::lifetime::construct_sampler(&sampler, device, SAMPLER_INFO);
        const auto third = std::unique_ptr<void, decltype(&vsl::lifetime::destroy_sampler)>(&sampler, vsl::lifetime::destroy_sampler);
        auto& a = vsl::lifetime::image_at(&images[0]);
        auto& b = vsl::lifetime::image_at(&images[1]);
        auto& c = vsl::lifetime::sampler_at(&sampler);
        EXPECT_NE(a.image, b.image);
        EXPECT_NE(a.use, b.use);
        EXPECT_NE(a.use, c.use);
        EXPECT_EQ(a.device, device.data_.get());
        EXPECT_TRUE(a.image && a.memory && a.view && c.sampler);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(&a) % alignof(vsl::impl::ImageImpl), 0u);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(c.use) % alignof(vsl::ResourceUseState), 0u);
    }
    TEST_F(ImageStorageVulkanIntegrationTest, DescriptorFactoriesAndGettersKeepExternalCompositeOwnersAndSuppliedUses) {
        using Value = t::detail::DescriptorValue;
        struct Composite {
            vsl::ImageAccessor image;
            vsl::SamplerAccessor sampler;
            std::shared_ptr<int> token;
        };
        std::vector<Value> values;
        std::weak_ptr<Composite> owner;
        std::weak_ptr<int> token;
        const vsl::ResourceUse supplied{std::make_shared<vsl::ResourceUseState>(), std::make_shared<int>(3)};
        {
            vsl::Image<IMAGE_USAGE> image(device, 1, 1);
            vsl::Sampler sampler(device);
            auto context = std::make_shared<Composite>(Composite{image, sampler, std::make_shared<int>(17)});
            owner = context;
            token = context->token;
            const vsl::ImageAccessor external_image = ImageAccess::FromShared(std::shared_ptr<vsl::impl::ImageImpl>(context, ImageAccess::Owned(image).get()));
            const vsl::SamplerAccessor external_sampler = SamplerAccess::FromShared(std::shared_ptr<vsl::impl::SamplerImpl>(context, SamplerAccess::Owned(sampler).get()));
            const auto image_use = vsl::ResourceUse::ForImage(external_image);
            const auto sampler_use = vsl::ResourceUse::ForSampler(external_sampler);
            EXPECT_EQ(image_use.state.get(), &image.use_state());
            EXPECT_EQ(sampler_use.state.get(), &sampler.use_state());
            for (const auto& use : {image_use, sampler_use}) {
                EXPECT_FALSE(use.owner.owner_before(context));
                EXPECT_FALSE(context.owner_before(use.owner.share()));
            }
            values.push_back(Value::FromImage(external_image, supplied));
            values.push_back(Value::FromSampler(external_sampler, supplied));
            values.push_back(Value::FromTexture(external_image, external_sampler, supplied));
            for (const auto& value : values) {
                EXPECT_EQ(value.resource_use().state, supplied.state);
                EXPECT_FALSE(value.resource_use().owner.owner_before(supplied.owner));
                EXPECT_FALSE(supplied.owner.owner_before(value.resource_use().owner));
            }
            for (const auto index : {0u, 2u}) {
                const auto retained = values[index].image();
                EXPECT_FALSE(ImageAccess::Owned(retained).owner_before(context));
                EXPECT_FALSE(context.owner_before(ImageAccess::Owned(retained).share()));
            }
            for (const auto index : {1u, 2u}) {
                const auto retained = values[index].sampler();
                EXPECT_FALSE(SamplerAccess::Owned(retained).owner_before(context));
                EXPECT_FALSE(context.owner_before(SamplerAccess::Owned(retained).share()));
            }
        }
        EXPECT_FALSE(owner.expired());
        auto image = values.back().image();
        auto sampler = values.back().sampler();
        values.clear();
        EXPECT_FALSE(token.expired());
        ImageAccess::Owned(image).reset();
        EXPECT_FALSE(token.expired());
        SamplerAccess::Owned(sampler).reset();
        EXPECT_TRUE(owner.expired());
        EXPECT_TRUE(token.expired());
    }

    TEST_F(ImageStorageVulkanIntegrationTest, FailedConstructionReturnsTheSlotToAnUnconstructedReusableState) {
        vsl::lifetime::ImageSlotStorage image;
        auto invalid = IMAGE_INFO;
        invalid.properties = vsl::MemoryProperty::HOST_VISIBLE | vsl::MemoryProperty::LAZILY_ALLOCATED;
        EXPECT_THROW(vsl::lifetime::construct_image(&image, device, invalid), vsl::exceptions::RuntimeException);
        invalid = IMAGE_INFO;
        invalid.width = 0;
        EXPECT_THROW(vsl::lifetime::construct_image(&image, device, invalid), std::invalid_argument);
        ASSERT_NO_THROW(vsl::lifetime::construct_image(&image, device, IMAGE_INFO));
        const auto owned_image = std::unique_ptr<void, decltype(&vsl::lifetime::destroy_image)>(&image, vsl::lifetime::destroy_image);
        EXPECT_TRUE(vsl::lifetime::image_at(&image).image);
        vsl::lifetime::SamplerSlotStorage sampler;
        EXPECT_THROW(vsl::lifetime::construct_sampler(&sampler, {}, SAMPLER_INFO), std::invalid_argument);
        EXPECT_THROW(vsl::lifetime::construct_sampler(&sampler, device, {static_cast<vsl::SamplingMode>(9)}), std::invalid_argument);
        ASSERT_NO_THROW(vsl::lifetime::construct_sampler(&sampler, device, SAMPLER_INFO));
        const auto owned_sampler = std::unique_ptr<void, decltype(&vsl::lifetime::destroy_sampler)>(&sampler, vsl::lifetime::destroy_sampler);
        EXPECT_TRUE(vsl::lifetime::sampler_at(&sampler).sampler);
    }
    TEST_F(ImageStorageVulkanIntegrationTest, BorrowedNativeImageSurvivesTheDestructionOfItsAdditionalView) {
        vsl::Image<IMAGE_USAGE> image(device, 2, 2, IMAGE_INFO.format);
        vsl::lifetime::ImageSlotStorage borrowed;
        vsl::lifetime::construct_image(&borrowed, device, IMAGE_INFO, ImageAccess::Owned(image)->image);
        const auto& ref = vsl::lifetime::image_at(&borrowed);
        EXPECT_EQ(ref.image, ImageAccess::Owned(image)->image);
        EXPECT_NE(ref.view, ImageAccess::Owned(image)->view);
        EXPECT_FALSE(ref.owns_image);
        EXPECT_FALSE(ref.memory);
        vsl::lifetime::destroy_image(&borrowed);
        EXPECT_GT(device.data_->device.getImageMemoryRequirements(ImageAccess::Owned(image)->image).size, 0u);
    }
    TEST_F(ImageStorageVulkanIntegrationTest, DeviceAccessorDoesNotKeepItsSourceImageAlive) {
        vsl::LogicalDeviceAccessor retained_device;
        std::weak_ptr<vsl::impl::ImageImpl> owner;
        {
            vsl::Image<IMAGE_USAGE> image(device, 2, 2, IMAGE_INFO.format);
            owner = ImageAccess::Owned(image).share();
            retained_device = image.get_device();
            EXPECT_EQ(&image.use_state(), image.resource_use().state.get());
        }
        EXPECT_TRUE(owner.expired());
        EXPECT_EQ(retained_device, device);
    }
    TEST_F(ImageStorageVulkanIntegrationTest, NativeDescriptorReplacementTracksTheCurrentImageAndSamplerStates) {
        using Set = t::DescriptorSet<0, t::TextureBinding<0, vsl::ShaderFlag::COMPUTE>>;
        vsl::GraphicResourceManager descriptors(device);
        t::TypedDescriptorSet<Set> set(device, descriptors, vsl::ResourceRetention::COMPLETION);
        vsl::Image<IMAGE_USAGE> first(device, 2, 2, IMAGE_INFO.format), second(device, 2, 2, IMAGE_INFO.format);
        vsl::Sampler sampler(device, SAMPLER_INFO.sampling, SAMPLER_INFO.edge);
        set.write<0>(t::TypedTexture(first, sampler));
        auto native = set.backend();
        native.update(second, sampler, 0);
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        set.resource_use()->record(manager.get_buffer());
        set.resource_use()->record(manager.get_buffer());
        EXPECT_FALSE(first.use_state().busy());
        EXPECT_TRUE(second.use_state().busy());
        EXPECT_TRUE(sampler.use_state().busy());
        EXPECT_EQ(phase.completion().data_->work.lock()->completion_resources.size(), 3u);
        EXPECT_THROW(set.write<0>(t::TypedTexture(first, sampler)), std::logic_error);
        phase.cancel();
        EXPECT_FALSE(second.use_state().busy());
        EXPECT_FALSE(sampler.use_state().busy());
    }
    TEST_F(ImageStorageVulkanIntegrationTest, TypedComputeKeepsImagesAndSamplersUntilCompletionWithoutKeepingWrappers) {
        using Set = t::DescriptorSet<0, t::StorageBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>,
                                     t::DescriptorBinding<1, vsl::graphic_resource::Type::SAMPLED_IMAGE, vsl::ShaderFlag::COMPUTE>,
                                     t::DescriptorBinding<2, vsl::graphic_resource::Type::SAMPLER, vsl::ShaderFlag::COMPUTE>>;
        vsl::GraphicResourceManager descriptors(device);
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("reflection_separate_images.comp.spv"));
        t::TypedComputePipeline<t::ResourceSchema<Set>> pipeline(device, vsl::pipeline_layout::ShaderGroup("placed image", {shader}));
        t::TypedStorageBuffer<std::uint32_t> output(device, std::span<const std::uint32_t>(std::array<std::uint32_t, 2>{}));
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        std::weak_ptr<vsl::impl::ImageImpl> image_owner;
        std::weak_ptr<vsl::impl::SamplerImpl> sampler_owner;
        {
            vsl::Image<IMAGE_USAGE> image(device, 1, 1, IMAGE_INFO.format);
            vsl::Sampler sampler(device, SAMPLER_INFO.sampling, SAMPLER_INFO.edge);
            image_owner = ImageAccess::Owned(image).share();
            sampler_owner = SamplerAccess::Owned(sampler).share();
            {
                const std::array<std::uint8_t, 4> color{37, 83, 0, 255};
                vsl::StagingBuffer<> pixels(device, color);
                auto upload = make_manager();
                auto initialization = upload.start_phase<vsl::ComputePhase>();
                initialization << vsl::command::ImageUpload(&pixels, image, vsl::ImageLayout::UNDEFINED);
                ASSERT_EQ(initialization.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                ASSERT_TRUE(initialization.completion().wait());
            }
            t::TypedDescriptorSet<Set> set(device, descriptors, vsl::ResourceRetention::COMPLETION);
            set.write<0>(output);
            set.write<1>(t::TypedSampledImage(image));
            set.write<2>(t::TypedSampler(sampler));
            auto command = pipeline.bind_resources(set).dispatch(1);
            phase << command;
            EXPECT_TRUE(image.use_state().busy());
            EXPECT_TRUE(sampler.use_state().busy());
        }
        EXPECT_FALSE(image_owner.expired());
        EXPECT_FALSE(sampler_owner.expired());
        const vk::MemoryBarrier host_visible{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
        auto buffer = manager.get_buffer();
        CommandAccess::Data(buffer)->command_buffers.at(buffer.get_current_buffer_idx()).pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, host_visible, {}, {});
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        EXPECT_TRUE(image_owner.expired());
        EXPECT_TRUE(sampler_owner.expired());
        std::array<std::uint32_t, 2> actual{};
        output.read_into(actual);
        EXPECT_EQ(actual, (std::array<std::uint32_t, 2>{37, 83}));
    }
    TEST_F(ImageStorageVulkanIntegrationTest, RenderPassRegistersThePhysicalStateOfEveryAttachment) {
        vsl::RenderPassDescription description;
        description.add_color({.format = IMAGE_INFO.format});
        vsl::RenderPass pass(device, description);
        vsl::Image<vsl::ImageType::COLOR_ATTACHMENT> image(device, 2, 2, IMAGE_INFO.format);
        vsl::FrameBuffer target(pass, 2, 2, {image});
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << vsl::command::RenderPassBegin(target) << vsl::command::RenderPassEnd{};
        EXPECT_TRUE(image.use_state().busy());
        EXPECT_EQ(phase.completion().data_->work.lock()->resources.size(), 1u);
        phase.cancel();
        EXPECT_FALSE(image.use_state().busy());
    }
    TEST_F(ImageStorageVulkanIntegrationTest, SavedBarrierKeepsItsImageThroughRecordingAndCancellation) {
        std::optional<vsl::command::ChangeImageBarrier> command;
        std::weak_ptr<vsl::impl::ImageImpl> owner;
        {
            vsl::Image<IMAGE_USAGE> image(device, 2, 2, IMAGE_INFO.format);
            owner = ImageAccess::Owned(image).share();
            command.emplace(image, vsl::ImageLayout::UNDEFINED, vsl::ImageLayout::TRANSFER_DST_OPTIMAL);
        }
        EXPECT_FALSE(owner.expired());
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << *command;
        command.reset();
        ASSERT_FALSE(owner.expired());
        EXPECT_TRUE(owner.lock()->use->busy());
        phase.cancel();
        ASSERT_FALSE(owner.expired());
        EXPECT_FALSE(owner.lock()->use->busy());
    }
} // namespace
