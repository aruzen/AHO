#include "support/vulkan_fixture.hpp"
#include <VSL/lifetime/image_storage.hpp>
#include <VSL/lifetime/sampler_storage.hpp>
#include <VSL/utils/typed_compute.hpp>
#include <VSL/vulkan/commands/image_upload.hpp>
#include <VSL/vulkan/commands/render_pass_begin.hpp>
#include <VSL/vulkan/commands/render_pass_end.hpp>

namespace {
    namespace t = vsl::typed;
    constexpr auto IMAGE_USAGE = vsl::ImageType::SAMPLED | vsl::ImageType::TRANSFER_DST;
    const vsl::ImageCreateInfo IMAGE_INFO{IMAGE_USAGE, 2, 2, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA};
    const vsl::SamplerCreateInfo SAMPLER_INFO{vsl::SamplingMode::NEAREST, vsl::EdgeMode::CLAMP_TO_EDGE};
    class ImageStorageVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };

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
            const vsl::ImageAccessor external_image{std::shared_ptr<vsl::impl::ImageImpl>(context, image.data_.get())};
            const vsl::SamplerAccessor external_sampler{std::shared_ptr<vsl::impl::SamplerImpl>(context, sampler.data_.get())};
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
                EXPECT_FALSE(retained.data_.owner_before(context));
                EXPECT_FALSE(context.owner_before(retained.data_));
            }
            for (const auto index : {1u, 2u}) {
                const auto retained = values[index].sampler();
                EXPECT_FALSE(retained.data_.owner_before(context));
                EXPECT_FALSE(context.owner_before(retained.data_));
            }
        }
        EXPECT_FALSE(owner.expired());
        auto image = values.back().image();
        auto sampler = values.back().sampler();
        values.clear();
        EXPECT_FALSE(token.expired());
        image.data_.reset();
        EXPECT_FALSE(token.expired());
        sampler.data_.reset();
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
        vsl::lifetime::construct_image(&borrowed, device, IMAGE_INFO, image.data_->image);
        const auto& ref = vsl::lifetime::image_at(&borrowed);
        EXPECT_EQ(ref.image, image.data_->image);
        EXPECT_NE(ref.view, image.data_->view);
        EXPECT_FALSE(ref.owns_image);
        EXPECT_FALSE(ref.memory);
        vsl::lifetime::destroy_image(&borrowed);
        EXPECT_GT(device.data_->device.getImageMemoryRequirements(image.data_->image).size, 0u);
    }
    TEST_F(ImageStorageVulkanIntegrationTest, DeviceAccessorDoesNotKeepItsSourceImageAlive) {
        vsl::LogicalDeviceAccessor retained_device;
        std::weak_ptr<vsl::impl::ImageImpl> owner;
        {
            vsl::Image<IMAGE_USAGE> image(device, 2, 2, IMAGE_INFO.format);
            owner = image.data_;
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
            image_owner = image.data_;
            sampler_owner = sampler.data_;
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
        buffer.data_->command_buffers.at(buffer.get_current_buffer_idx()).pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, host_visible, {}, {});
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
            owner = image.data_;
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
