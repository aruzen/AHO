#include <VSL/lifetime/command_access.hpp>
#include <VSL/utils/typed_pipeline.hpp>
#include "support/render_fixture.hpp"
#include <VSL/utils/typed_compute.hpp>
#include <VSL/vulkan/commands/copy_buffer_to_image.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace {
    namespace t = vsl::typed;
    struct Tint {
        float rgba[4];
    };

    using TintRecord = t::BlockRecord<Tint, VSL_BLOCK_MEMBER(Tint, rgba)>;
    struct WrongTint {
        float rgba[4];
    };
    struct DescriptorTestValue {
        using Payload = Tint;
        static constexpr auto KIND = vsl::graphic_resource::Type::UNIFORM_BUFFER;
        t::detail::DescriptorValue value;
        t::detail::DescriptorValue descriptor() const { return value; }
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

    TEST_F(TypedDescriptorComputeVulkanIntegrationTest, BothStoragePoliciesKeepZeroInitializationFixedCountsAndTypedReads) {
        auto verify = [&](auto policy) {
            using Buffer = t::TypedStorageBuffer<std::uint32_t, vsl::MemoryProperty::HOST_VISIBLE>;
            Buffer buffer(device, std::in_place, 4, policy);
            std::array<std::uint32_t, 4> actual{99, 99, 99, 99};
            buffer.read_into(actual);
            EXPECT_EQ(actual, (std::array<std::uint32_t, 4>{0, 0, 0, 0}));
            const std::array<std::uint32_t, 4> values{3, 5, 7, 11};
            buffer.copy(values);
            std::array<std::uint32_t, 2> selected{};
            buffer.read_into(selected, 1);
            EXPECT_EQ(selected, (std::array<std::uint32_t, 2>{5, 7}));
            EXPECT_THROW(buffer.copy(std::span(values).first(3)), std::invalid_argument);
            EXPECT_THROW(buffer.read_into(selected, 3), std::out_of_range);
            buffer.read_into(actual);
            EXPECT_EQ(actual, values);
            EXPECT_EQ(buffer.backend().size(), sizeof(values));
            EXPECT_TRUE(vsl::contain(buffer.backend().ref().properties(), vsl::MemoryProperty::HOST_VISIBLE));
            EXPECT_THROW((Buffer(device, std::in_place, 0, policy)), vsl::exceptions::RuntimeException);
        };
        verify(t::HeapStorage{});
        verify(t::ArenaStorage<>{});
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

    TEST_F(TypedDescriptorVulkanIntegrationTest, NativeUpdatesShareElementInitializationAndPreserveArrayOwnersAndPixels) {
        auto typed_pipeline = make_typed<Schema>();
        vsl::GraphicResourceManager manager(device);
        t::TypedDescriptorSet<Tints> tints(device, manager);
        t::TypedDescriptorSet<Extra> extra(device, manager);
        auto native = tints.backend();
        const Tint replacement{{.7f, .1f, .3f, 1}}, zero{};
        t::TypedUniformBuffer<Tint> updated(device, replacement);
        t::TypedStorageBuffer<Tint> storage(device, zero);
        auto native_extra = extra.backend();
        EXPECT_THROW(native.update(storage.backend(), 3), std::invalid_argument);
        EXPECT_THROW(tints.require_complete(), std::logic_error);
        const auto alignment = physical.data_->props->limits.minUniformBufferOffsetAlignment;
        if (alignment > 1)
            EXPECT_THROW(native.update(updated.backend(), 3, std::nullopt, 1, sizeof(float)), std::invalid_argument);
        native_extra.update(storage.backend(), 5);
        EXPECT_NO_THROW(extra.require_complete());
        native.update(updated.backend(), 3);
        EXPECT_THROW(tints.require_complete(), std::logic_error);
        std::weak_ptr<void> first_owner, second_owner;
        {
            const Tint first{{.2f, .4f, .6f, 1}}, second{{.3f, .6f, .9f, 1}};
            t::TypedUniformBuffer<Tint> a(device, first), b(device, second);
            first_owner = a.storage_view().owner();
            second_owner = b.storage_view().owner();
            tints.write<3>(std::array{a, b});
        }
        EXPECT_FALSE(first_owner.expired());
        EXPECT_FALSE(second_owner.expired());
        native.update(updated.backend(), 3);
        EXPECT_TRUE(first_owner.expired());
        EXPECT_FALSE(second_owner.expired());
        EXPECT_NO_THROW(tints.require_complete());
        const auto pixel = render([&](auto& stream) { stream << typed_pipeline.bind_vertices().bind_resources(tints, extra).draw(3); });
        EXPECT_NEAR(pixel[0], 128, 1);
        EXPECT_NEAR(pixel[1], 89, 1);
        EXPECT_NEAR(pixel[2], 153, 1);
        EXPECT_EQ(pixel[3], 255);
        native = {};
        tints = t::TypedDescriptorSet<Tints>(device, manager);
        EXPECT_TRUE(second_owner.expired());
    }

    TEST_F(TypedDescriptorVulkanIntegrationTest, InvalidArrayTailKeepsThePreviousOwnersInitializationAndGPUValues) {
        auto typed_pipeline = make_typed<Schema>();
        vsl::GraphicResourceManager manager(device);
        t::TypedDescriptorSet<Tints> tints(device, manager);
        t::TypedDescriptorSet<Extra> extra(device, manager);
        const Tint original{{.25f, .5f, .75f, 1}}, replacement{{1, 0, 0, 1}}, zero{};
        t::TypedStorageBuffer<Tint> storage(device, zero);
        extra.write<5>(storage);
        std::weak_ptr<void> previous;
        {
            t::TypedUniformBuffer<Tint> buffer(device, original);
            previous = buffer.storage_view().owner();
            tints.write<3>(std::array{buffer, buffer});
        }
        t::TypedUniformBuffer<Tint> buffer(device, replacement);
        EXPECT_THROW(tints.write<3>(std::array{DescriptorTestValue{buffer.descriptor()}, DescriptorTestValue{}}), std::invalid_argument);
        EXPECT_FALSE(previous.expired());
        EXPECT_NO_THROW(tints.require_complete());
        ExpectTriangleColor(render([&](auto& stream) { stream << typed_pipeline.bind_vertices().bind_resources(tints, extra).draw(3); }));
    }

    TEST_F(TypedDescriptorVulkanIntegrationTest, DestroyedPoolInvalidatesInitializationIncludingAnEmptyLayout) {
        vsl::GraphicResourceManager manager(device);
        t::TypedDescriptorSet<Tints> set(device, manager);
        const Tint value{{.25f, .5f, .75f, 1}};
        t::TypedUniformBuffer<Tint> buffer(device, value);
        set.write<3>(std::array{buffer, buffer});
        auto native = set.backend();
        manager.destroy(native.get_pool());
        EXPECT_TRUE(native.data_->bindings.empty());
        EXPECT_THROW(set.require_complete(), std::logic_error);
        EXPECT_THROW(set.write<3>(std::array{buffer, buffer}), std::logic_error);
        t::TypedDescriptorSet<t::DescriptorSet<0>> empty(device, manager);
        EXPECT_NO_THROW(empty.require_complete());
        native = empty.backend();
        manager.destroy(native.get_pool());
        EXPECT_THROW(empty.require_complete(), std::logic_error);
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
        std::weak_ptr<void> weak_buffer;
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
                weak_buffer = buffer.storage_view().owner();
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
            auto native = set.backend();
            EXPECT_THROW(native.update(image, 2), std::invalid_argument);
            EXPECT_THROW(set.require_complete(), std::logic_error);
            weak_image = image.data_;
            weak_sampler = local.data_;
            set.write<2>(std::array{t::TypedTexture(image, sampler), t::TypedTexture(image, local)});
            EXPECT_THROW(native.update(image, 2), std::invalid_argument);
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

    TEST_F(TypedDescriptorComputeVulkanIntegrationTest, ComputeDraftRejectsStageAndSparseSetErrorsBeforeMaterialization) {
        std::optional<vsl::PipelineLayoutEditAccessor> saved;
        struct Capture {
            std::optional<vsl::PipelineLayoutEditAccessor>& saved;
            void injection(vsl::PipelineLayoutEditAccessor layout) const { saved = std::move(layout); }
        };
        using EmptySet = t::ResourceSchema<t::DescriptorSet<0>>;
        using HugeSet = t::ResourceSchema<t::DescriptorSet<0x10000000>>;
        vsl::Shader<vsl::ShaderType::VERTEX> vertex(device, vsl_test::shader_path("vertices.vert.spv"));
        auto verify = [&](auto policy) {
            EXPECT_THROW((t::TypedComputePipeline<EmptySet>(policy, device, Capture{saved})), std::invalid_argument);
            ASSERT_TRUE(saved);
            EXPECT_FALSE(saved->data_);
            ASSERT_TRUE(saved->edit_recipe().pending_layouts);
            EXPECT_EQ(saved->edit_recipe().pending_layouts->size(), 1u);
            EXPECT_THROW((t::TypedComputePipeline<EmptySet>(policy, device, Capture{saved},
                                                            vsl::pipeline_layout::ShaderGroup("not compute", {vertex}))),
                         std::invalid_argument);
            EXPECT_FALSE(saved->data_);
            EXPECT_THROW((t::TypedComputePipeline<HugeSet>(policy, device, Capture{saved})), std::out_of_range);
            EXPECT_FALSE(saved->data_);
            EXPECT_FALSE(saved->edit_recipe().pending_layouts);
            saved.reset();
        };
        verify(t::HeapStorage{});
        verify(t::ArenaStorage<>{});
    }

    TEST_F(TypedDescriptorComputeVulkanIntegrationTest, TypedUniformAndStorageSchemasSurviveDispatchAndOwnTheirPayloads) {
        struct Parameters {
            std::uint32_t base;
        };
        using ParametersRecord = t::BlockRecord<Parameters, VSL_BLOCK_MEMBER(Parameters, base)>;
        using ParametersSet = t::DescriptorSet<0, t::UniformBinding<2, ParametersRecord, vsl::ShaderFlag::COMPUTE>>;
        using OutputSet = t::DescriptorSet<1, t::StorageBinding<4, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
        using ComputeSchema = t::ResourceSchema<OutputSet, ParametersSet>;
        auto verify = [&](auto policy) {
            std::weak_ptr<vsl::impl::ShaderImpl> shader_owner;
            auto typed_pipeline = [&] {
                vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_descriptors.comp.spv"));
                shader_owner = shader.data_;
                return t::TypedComputePipeline<ComputeSchema>(policy, device, vsl::pipeline_layout::ShaderGroup("typed compute", {shader}));
            }();
            EXPECT_TRUE(shader_owner.expired());
            const auto& runtime = typed_pipeline.backend().data_;
            EXPECT_EQ(runtime->owner_node == runtime->layout->owner_node, (std::same_as<decltype(policy), t::ArenaStorage<>>));
            vsl::GraphicResourceManager manager(device);
            t::TypedDescriptorSet<ParametersSet> input(device, manager);
            t::TypedDescriptorSet<OutputSet> output(device, manager);
            const Parameters parameters{491};
            const std::array<std::uint32_t, 16> zero{};
            t::TypedUniformBuffer<Parameters> uniform(device, parameters, policy);
            t::TypedStorageBuffer<std::uint32_t> storage(device, zero, policy);
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
                CommandAccess::Data(buffer)->command_buffers[buffer.get_current_buffer_idx()].pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
            }
            device.data_->device.waitIdle();
            std::array<std::uint32_t, zero.size()> actual{};
            storage.read_into(actual);
            for (size_t i = 0; i < zero.size(); ++i)
                EXPECT_EQ(actual[i], parameters.base + i);
        };
        verify(t::HeapStorage{});
        verify(t::ArenaStorage<>{});
    }

} // namespace
