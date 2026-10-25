#include <VSL/utils/typed_pipeline.hpp>
#include "support/render_fixture.hpp"
#include <VSL/utils/typed_compute.hpp>
#include <VSL/vulkan/commands/copy_buffer_to_image.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>

namespace {
    namespace t = vsl::typed;
    struct Tint {
        float rgba[4];
    };

    using TintRecord = t::BlockRecord<Tint, VSL_BLOCK_MEMBER(Tint, rgba)>;
    struct WrongTint {
        float rgba[4];
    };
    using Tints = t::DescriptorSet<2, t::UniformBinding<3, TintRecord, vsl::ShaderFlag::VERTEX, 2>>;
    using Extra = t::DescriptorSet<0, t::StorageBinding<5, TintRecord, vsl::ShaderFlag::VERTEX>>;
    using Schema = t::ResourceSchema<Tints, Extra>;
    using EmptyInput = t::TypedVertexInput<>;
    static_assert(Schema::SET_COUNT == 3 && Schema::COUNT == 2);
    static_assert(t::compatible_descriptor_sets<Schema, t::TypedDescriptorSet<Tints>, t::TypedDescriptorSet<Extra>>);
    static_assert(!t::compatible_descriptor_sets<Schema, t::TypedDescriptorSet<Extra>, t::TypedDescriptorSet<Tints>>);
    static_assert(!t::compatible_descriptor_sets<Schema, t::TypedDescriptorSet<Tints>>);
    static_assert(!t::compatible_descriptor_sets<Schema, int, int>);

    TEST(TypedResourceSchemaTest, RequirementsRetainSparseSetBindingKindAndArrayCount) {
        constexpr auto values = Schema::REQUIREMENTS;
        ASSERT_EQ(values.size(), 2u);
        EXPECT_EQ(values[0].set, 2u);
        EXPECT_EQ(values[0].point.binding, 3u);
        EXPECT_EQ(values[0].point.amount, 2u);
        EXPECT_EQ(values[1].point.binding_type, vsl::graphic_resource::Type::STORAGE_BUFFER);
    }

    class TypedDescriptorVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }

    protected:
        template<typename Resources>
        auto make_typed(const char* shader = "typed_descriptors.vert.spv");
    };
    class TypedDescriptorComputeVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    template<typename Resources>
    auto TypedDescriptorVulkanIntegrationTest::make_typed(const char* shader) {
        namespace pl = vsl::pipeline_layout;
        vsl::Shader<vsl::ShaderType::VERTEX> source(device, vsl_test::shader_path(shader));
        return t::TypedGraphicsPipeline<EmptyInput, Resources>(device, *pass,
                                                               pl::ShaderGroup("typed resources", {source, *fragment}), pl::ColorBlend{}, pl::InputAssembly{},
                                                               pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{}, pl::DynamicState{},
                                                               vsl::Viewport(description), vsl::Scissor(description));
    }

    TEST_F(TypedDescriptorVulkanIntegrationTest, SparseSetsAndUniformArraysReachTheRealShader) {
        auto typed_pipeline = make_typed<Schema>();
        vsl::GraphicResourceManager manager(device);
        t::TypedDescriptorSet<Tints> tints(device, manager);
        t::TypedDescriptorSet<Extra> extra(device, manager);
        const Tint first{{.2f, .4f, .6f, 1}}, second{{.3f, .6f, .9f, 1}}, zero{};
        t::TypedUniformBuffer<Tint> first_buffer(device, first), second_buffer(device, second);
        t::TypedStorageBuffer<Tint> extra_buffer(device, zero);
        tints.write<3>(std::array{first_buffer, second_buffer});
        extra.write<5>(extra_buffer);
        auto bound = typed_pipeline.bind_vertices().bind_resources(tints, extra);
        ExpectTriangleColor(render([&](auto& stream) { stream << bound.draw(3); }));
        using Relocated = t::ResourceSchema<t::DescriptorSetBinding<3, Tints::Layout>, t::DescriptorSetBinding<1, Extra::Layout>>;
        auto relocated = make_typed<Relocated>("typed_descriptors_relocated.vert.spv");
        const auto native_set = tints.backend().data_;
        ExpectTriangleColor(render([&](auto& stream) { stream << relocated.bind_vertices().bind_resources(tints, extra).draw(3); }));
        EXPECT_EQ(native_set, tints.backend().data_);
        const Tint update{{.7f, .1f, .3f, 1}};
        first_buffer.copy(std::span(&update, 1));
        auto pixel = render([&](auto& stream) { stream << bound.draw(3); });
        EXPECT_NEAR(pixel[0], 128, 1);
        EXPECT_NEAR(pixel[1], 89, 1);
        EXPECT_NEAR(pixel[2], 153, 1);
    }

    TEST_F(TypedDescriptorVulkanIntegrationTest, MissingInitializationIsRejectedAndCopiesShareInitializationState) {
        auto typed_pipeline = make_typed<Schema>();
        vsl::GraphicResourceManager manager(device);
        t::TypedDescriptorSet<Tints> tints(device, manager);
        t::TypedDescriptorSet<Extra> extra(device, manager);
        EXPECT_THROW(typed_pipeline.bind_vertices().bind_resources(tints, extra), std::logic_error);
        auto alias = tints;
        const Tint value{{.25f, .5f, .75f, 1}}, zero{};
        t::TypedUniformBuffer<Tint> buffer(device, value);
        t::TypedStorageBuffer<Tint> storage(device, zero);
        tints.write<3>(std::array{buffer, buffer});
        extra.write<5>(storage);
        ExpectTriangleColor(render([&](auto& stream) {
            stream << typed_pipeline.bind_vertices().bind_resources(alias, extra).draw(3);
        }));
    }

    TEST_F(TypedDescriptorVulkanIntegrationTest, ShaderSchemaMismatchIsRejectedBeforeGraphicsPipelineCreation) {
        using WrongArray = t::DescriptorSet<2, t::UniformBinding<3, TintRecord, vsl::ShaderFlag::VERTEX>>;
        using WrongStage = t::DescriptorSet<2, t::UniformBinding<3, TintRecord, vsl::ShaderFlag::FRAGMENT, 2>>;
        using WrongKind = t::DescriptorSet<2, t::StorageBinding<3, TintRecord, vsl::ShaderFlag::VERTEX, 2>>;
        EXPECT_THROW((make_typed<t::ResourceSchema<WrongArray, Extra>>()), std::invalid_argument);
        EXPECT_THROW((make_typed<t::ResourceSchema<WrongStage, Extra>>()), std::invalid_argument);
        EXPECT_THROW((make_typed<t::ResourceSchema<WrongKind, Extra>>()), std::invalid_argument);
        EXPECT_THROW(make_typed<t::ResourceSchema<>>(), std::invalid_argument);
        EXPECT_FALSE(has_validation_errors());
    }

    TEST_F(TypedDescriptorVulkanIntegrationTest, DrawOwnsDescriptorResourcesAndTheirBuffersUntilGpuCompletion) {
        std::weak_ptr<vsl::impl::BufferImpl> weak_buffer;
        std::weak_ptr<vsl::impl::GraphicResourceImpl> weak_set;
        ExpectTriangleColor(render([&](auto& stream) {
            {
                auto typed_pipeline = make_typed<Schema>();
                vsl::GraphicResourceManager manager(device);
                t::TypedDescriptorSet<Tints> tints(device, manager);
                t::TypedDescriptorSet<Extra> extra(device, manager);
                const Tint value{{.25f, .5f, .75f, 1}}, zero{};
                t::TypedUniformBuffer<Tint> buffer(device, value);
                t::TypedStorageBuffer<Tint> storage(device, zero);
                weak_buffer = buffer.backend().data_;
                weak_set = tints.backend().data_;
                tints.write<3>(std::array{buffer, buffer});
                extra.write<5>(storage);
                stream << typed_pipeline.bind_vertices().bind_resources(tints, extra).draw(3);
            }
            EXPECT_FALSE(weak_buffer.expired());
            EXPECT_FALSE(weak_set.expired());
        }));
        // The fixture waits for the GPU; the last completion may still own recorded objects.
        EXPECT_FALSE(has_validation_errors());
    }

    TEST_F(TypedDescriptorVulkanIntegrationTest, ImageUsageIsValidatedAndDescriptorArraysRetainEveryImageAndSampler) {
        using Textures = t::DescriptorSet<1, t::TextureBinding<2, vsl::ShaderFlag::FRAGMENT, 2>>;
        vsl::GraphicResourceManager manager(device);
        t::TypedDescriptorSet<Textures> set(device, manager);
        vsl::Image<vsl::ImageType::COLOR_ATTACHMENT> invalid(device, 1, 1, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
        vsl::Sampler sampler(device, vsl::SamplingMode::NEAREST, vsl::EdgeMode::CLAMP_TO_EDGE);
        EXPECT_THROW(set.write<2>(std::array{t::TypedTexture(invalid, sampler), t::TypedTexture(invalid, sampler)}), std::invalid_argument);
        std::weak_ptr<vsl::impl::ImageImpl> weak_image;
        std::weak_ptr<vsl::impl::SamplerImpl> weak_sampler;
        {
            vsl::Image<vsl::ImageType::SAMPLED> image(device, 1, 1, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
            vsl::Sampler local(device, vsl::SamplingMode::NEAREST, vsl::EdgeMode::CLAMP_TO_EDGE);
            weak_image = image.data_;
            weak_sampler = local.data_;
            set.write<2>(std::array{t::TypedTexture(image, sampler), t::TypedTexture(image, local)});
        }
        EXPECT_FALSE(weak_image.expired());
        EXPECT_FALSE(weak_sampler.expired());
        EXPECT_NO_THROW(set.require_complete());
    }
    TEST_F(TypedDescriptorVulkanIntegrationTest, TextureArraysSupplyBothImagesToTheFragmentShader) {
        using Textures = t::DescriptorSet<1, t::TextureBinding<2, vsl::ShaderFlag::FRAGMENT, 2>>;
        using TextureSchema = t::ResourceSchema<Textures>;
        using Image = vsl::Image<vsl::ImageType::SAMPLED | vsl::ImageType::TRANSFER_DST>;
        Image first(device, 1, 1, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
        Image second(device, 1, 1, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
        const std::array<std::uint8_t, 4> red{128, 0, 128, 255}, green{0, 255, 255, 255};
        vsl::StagingBuffer<> first_data(device, red), second_data(device, green);
        auto upload = make_manager();
        {
            auto phase = upload.start_phase<vsl::ComputePhase>();
            for (auto image : {vsl::ImageAccessor(first), vsl::ImageAccessor(second)})
                phase << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::UNDEFINED, vsl::ImageLayout::TRANSFER_DST_OPTIMAL);
            phase << vsl::command::CopyBufferToImage(first, &first_data, vsl::ImageLayout::TRANSFER_DST_OPTIMAL)
                  << vsl::command::CopyBufferToImage(second, &second_data, vsl::ImageLayout::TRANSFER_DST_OPTIMAL);
            for (auto image : {vsl::ImageAccessor(first), vsl::ImageAccessor(second)})
                phase << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL, vsl::ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        }
        device.data_->device.waitIdle();
        vsl::Sampler sampler(device, vsl::SamplingMode::NEAREST, vsl::EdgeMode::CLAMP_TO_EDGE);
        vsl::GraphicResourceManager manager(device);
        t::TypedDescriptorSet<Textures> textures(device, manager);
        textures.write<2>(std::array{t::TypedTexture(first, sampler), t::TypedTexture(second, sampler)});
        vsl::Shader<vsl::ShaderType::VERTEX> source(device, vsl_test::shader_path("triangle.vert.spv"));
        vsl::Shader<vsl::ShaderType::FRAGMENT> frag(device, vsl_test::shader_path("typed_texture.frag.spv"));
        namespace pl = vsl::pipeline_layout;
        t::TypedGraphicsPipeline<EmptyInput, TextureSchema> typed_pipeline(device, *pass,
                                                                           pl::ShaderGroup("typed textures", {source, frag}), pl::ColorBlend{}, pl::InputAssembly{},
                                                                           pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{}, pl::DynamicState{},
                                                                           vsl::Viewport(description), vsl::Scissor(description));
        ExpectTriangleColor(render([&](auto& stream) { stream << typed_pipeline.bind_vertices().bind_resources(textures).draw(3); }));
    }

    TEST_F(TypedDescriptorComputeVulkanIntegrationTest, TypedUniformAndStorageSchemasSurviveDispatchAndOwnTheirPayloads) {
        struct Parameters {
            std::uint32_t base;
        };
        using ParametersRecord = t::BlockRecord<Parameters, VSL_BLOCK_MEMBER(Parameters, base)>;
        using ParametersSet = t::DescriptorSet<0, t::UniformBinding<2, ParametersRecord, vsl::ShaderFlag::COMPUTE>>;
        using OutputSet = t::DescriptorSet<1, t::StorageBinding<4, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
        using ComputeSchema = t::ResourceSchema<OutputSet, ParametersSet>;
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_descriptors.comp.spv"));
        t::TypedComputePipeline<ComputeSchema> typed_pipeline(device, vsl::pipeline_layout::ShaderGroup("typed compute", {shader}));
        vsl::GraphicResourceManager manager(device);
        t::TypedDescriptorSet<ParametersSet> input(device, manager);
        t::TypedDescriptorSet<OutputSet> output(device, manager);
        const Parameters parameters{491};
        const std::array<std::uint32_t, 16> zero{};
        t::TypedUniformBuffer<Parameters> uniform(device, parameters);
        t::TypedStorageBuffer<std::uint32_t> storage(device, zero);
        input.write<2>(uniform);
        output.write<4>(storage);
        auto binding = typed_pipeline.bind_resources(output, input);
        EXPECT_THROW(binding.dispatch(size_t{UINT32_MAX} + 1), std::out_of_range);
        auto commands = make_manager();
        {
            auto phase = commands.start_phase<vsl::ComputePhase>();
            phase << binding.dispatch(zero.size());
            vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
            auto buffer = commands.get_buffer();
            buffer.data_->command_buffers[buffer.get_current_buffer_idx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
        }
        device.data_->device.waitIdle();
        auto mapped = storage.backend().data();
        const auto* actual = static_cast<const std::uint32_t*>(mapped.data);
        for (size_t i = 0; i < zero.size(); ++i)
            EXPECT_EQ(actual[i], parameters.base + i);
    }

} // namespace
