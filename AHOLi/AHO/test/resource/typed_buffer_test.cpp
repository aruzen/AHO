#ifdef AHO_GPU_TESTS
#include <AHO/resource/typed_buffer.hpp>
#include <AHO/x/resource/typed_texture.hpp>
#include <AHO/command/buffer_upload.hpp>
#include <AHO/x/command/texture_update.hpp>
#include <VSL/utils/typed_compute.hpp>
#include <VSL/utils/typed_pipeline.hpp>
#include <VSL/utils/typed_resource_bundle.hpp>
#include "support/render_fixture.hpp"

namespace {
    namespace r = aho::resource;
    namespace t = vsl::typed;
    using Host = r::Buffer<r::BufferType::GPU_FRIENDLY>;
    using Device = r::Buffer<r::BufferType::ON_GPU>;
    class TypedAHOBufferVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    struct Vertex {
        float xy[2];
        float rgb[3];
    };
    using Record = t::VertexRecord<Vertex, VSL_VERTEX_MEMBER(Vertex, xy), VSL_VERTEX_MEMBER(Vertex, rgb)>;
    using Input = t::TypedVertexInput<Record::Binding<0>>;
    using Set = t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>, t::StorageBinding<1, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
    using Schema = t::ResourceSchema<Set>;
    const std::array<Vertex, 4> VERTICES{{{{99, 99}, {0, 0, 0}}, {{-1, -1}, {.25f, .5f, .75f}}, {{3, -1}, {.25f, .5f, .75f}}, {{-1, 3}, {.25f, .5f, .75f}}}};

    TEST_F(TypedAHOBufferVulkanIntegrationTest, ViewsUseLogicalLengthAndPreserveTypedSubranges) {
        Host buffer(device, 64);
        const std::array<std::uint32_t, 3> values{1, 2, 3};
        buffer.assign(std::as_bytes(std::span(values)));
        EXPECT_EQ(r::uniform_view<std::uint32_t>(buffer).size(), 3u);
        EXPECT_EQ(r::storage_view<std::uint32_t>(buffer, 1).byte_offset(), 4u);
        const auto view = r::vertex_view<std::uint32_t>(buffer, 1, 2).view(1, 1);
        EXPECT_EQ(view.byte_offset(), 8u);
        EXPECT_EQ(view.size(), 1u);
        EXPECT_THROW(r::vertex_view<std::uint32_t>(buffer, 3), std::out_of_range);
        EXPECT_THROW(r::vertex_view<std::uint32_t>(buffer, 0, 4), std::out_of_range);
        EXPECT_THROW(r::uniform_view<std::uint32_t>(buffer, SIZE_MAX), std::out_of_range);
        EXPECT_THROW(r::storage_view<std::uint32_t>(buffer, 1, SIZE_MAX), std::out_of_range);
        buffer.resize(5);
        EXPECT_THROW(r::uniform_view<std::uint32_t>(buffer), std::invalid_argument);
        EXPECT_EQ(r::uniform_view<std::uint32_t>(buffer, 0, 1).size(), 1u);
        Device gpu(device, sizeof(Vertex));
        EXPECT_EQ(r::vertex_view<Vertex>(gpu).size(), 1u);
        buffer.resize(0);
        EXPECT_THROW(r::vertex_view<Vertex>(buffer), std::out_of_range);
    }

    TEST_F(TypedAHOBufferVulkanIntegrationTest, BindingOnlySourcesStillValidateNativeUsageAtRuntime) {
        struct BindingOnly {
            r::BufferBinding value;
            r::BufferBinding binding() const { return value; }
        };
        using Native = vsl::Buffer<vsl::MemoryType::TRANSFER_SOURCE, vsl::MemoryProperty::HOST_VISIBLE>;
        auto custom = r::make_custom_buffer(Native(device, 16));
        BindingOnly source{custom.binding()};
        EXPECT_THROW(r::uniform_view<float>(source), std::invalid_argument);
        EXPECT_THROW(r::vertex_view<float>(source), std::invalid_argument);
    }

    TEST_F(TypedAHOBufferVulkanIntegrationTest, DispatchTracksAhoBuffersAndBlocksCpuUpdatesUntilCompletion) {
        const std::uint32_t value = 37, zero = 0;
        Host uniform(device, sizeof(value)), output(device, sizeof(zero));
        uniform.write(std::as_bytes(std::span(&value, 1)));
        output.write(std::as_bytes(std::span(&zero, 1)));
        vsl::GraphicResourceManager descriptors(device);
        t::TypedDescriptorSet<Set> set(device, descriptors);
        set.write<0>(r::uniform_view<std::uint32_t>(uniform));
        set.write<1>(r::storage_view<std::uint32_t>(output));
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_frame.comp.spv"));
        t::TypedComputePipeline<Schema> pipeline(device, vsl::pipeline_layout::ShaderGroup("AHO buffers", {shader}));
        auto manager = make_manager().independent(1);
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << pipeline.bind_resources(set).dispatch(1);
        EXPECT_TRUE(uniform.busy());
        EXPECT_TRUE(output.busy());
        EXPECT_FALSE(output.wait(0));
        EXPECT_THROW(uniform.write({}), std::logic_error);
        EXPECT_THROW(uniform.assign({}), std::logic_error);
        EXPECT_THROW(uniform.reserve(128), std::logic_error);
        EXPECT_THROW(output.map(), std::logic_error);
        EXPECT_THROW(output.read(sizeof(zero)), std::logic_error);
        auto buffer = manager.get_buffer();
        vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
        buffer.data_->command_buffers[0].pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(output.wait());
        std::uint32_t result = 0;
        std::memcpy(&result, output.read(sizeof(result)).data(), sizeof(result));
        EXPECT_EQ(result, value);
        EXPECT_FALSE(uniform.busy());
        EXPECT_NO_THROW(uniform.write({}));
    }

    TEST_F(TypedAHOBufferVulkanIntegrationTest, ExternalUniformKeepsUsageStateThroughResourceBundle) {
        using Allocation = t::ResourceAllocationSchema<Schema, t::ExternalAllocation<0, 0>, t::BufferAllocation<0, 1>>;
        Host input(device, sizeof(std::uint32_t));
        const std::uint32_t value = 41;
        input.write(std::as_bytes(std::span(&value, 1)));
        auto manager = make_manager().independent(1);
        vsl::GraphicResourceManager descriptors(device);
        t::TypedResourceBundle<Allocation> bundle(manager, descriptors, {}, t::external_resource<0, 0>(r::uniform_view<std::uint32_t>(input)));
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_frame.comp.spv"));
        t::TypedComputePipeline<Schema> pipeline(device, vsl::pipeline_layout::ShaderGroup("AHO external", {shader}));
        auto phase = manager.start_phase<vsl::ComputePhase>();
        const auto& frame = bundle.get(phase);
        phase << frame.bind_resources(pipeline).dispatch(1);
        EXPECT_TRUE(input.busy());
        EXPECT_THROW(input.write({}), std::logic_error);
        auto buffer = manager.get_buffer();
        vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
        buffer.data_->command_buffers[0].pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        auto native = frame.buffer<0, 1>().backend();
        auto mapped = native.data(sizeof(value));
        EXPECT_EQ(*static_cast<const std::uint32_t*>(mapped.data), value);
        EXPECT_FALSE(input.busy());
        EXPECT_NO_THROW(input.write({}));
    }

    TEST_F(TypedAHOBufferVulkanIntegrationTest, UploadedDeviceVerticesCanBeSlicedAndDrawnWithTypedIndices) {
        r::Buffer<r::BufferType::GENERAL> cpu;
        cpu.assign(std::as_bytes(std::span(VERTICES)));
        Device vertices(device, sizeof(VERTICES));
        auto upload = make_manager();
        {
            auto phase = upload.start_phase<vsl::ComputePhase>();
            phase << vertices.upload(cpu);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(vertices.wait());
        }
        const std::array<std::uint16_t, 3> index_values{0, 1, 2};
        t::TypedIndexBuffer<std::uint16_t> indices(device, index_values);
        vsl::Shader<vsl::ShaderType::VERTEX> source(device, vsl_test::shader_path("vertices.vert.spv"));
        namespace pl = vsl::pipeline_layout;
        t::TypedGraphicsPipeline<Input> pipeline(device, *pass, pl::ShaderGroup("AHO vertices", {source, *fragment}),
                                                 pl::ColorBlend{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{}, pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description));
        ExpectTriangleColor(render([&](auto& stream) {
            stream << pipeline.bind_vertices(r::vertex_view<Vertex>(vertices, 1, 3)).draw_indexed(indices, 3);
            EXPECT_TRUE(vertices.busy());
            auto manager = upload.independent();
            auto phase = manager.start_phase<vsl::ComputePhase>();
            EXPECT_THROW(phase << vertices.upload(cpu), std::logic_error);
            phase.cancel();
        }));
        EXPECT_FALSE(vertices.busy());
    }

    TEST_F(TypedAHOBufferVulkanIntegrationTest, TypedDrawPinsCustomStorageAndCustomizationAfterWrappersAreDestroyed) {
        struct Customization {
            std::shared_ptr<int> owner;
        };
        using Native = vsl::Buffer<vsl::MemoryType::VERTEX_BUFFER, vsl::MemoryProperty::HOST_VISIBLE>;
        std::weak_ptr<int> owner;
        vsl::Shader<vsl::ShaderType::VERTEX> source(device, vsl_test::shader_path("vertices.vert.spv"));
        namespace pl = vsl::pipeline_layout;
        t::TypedGraphicsPipeline<Input> pipeline(device, *pass, pl::ShaderGroup("AHO ownership", {source, *fragment}),
                                                 pl::ColorBlend{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{}, pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description));
        ExpectTriangleColor(render([&](auto& stream) {
            {
                auto token = std::make_shared<int>(7);
                owner = token;
                auto custom = r::make_custom_buffer(Native(device, VERTICES), Customization{std::move(token)});
                auto view = r::vertex_view<Vertex>(custom, 1, 3);
                stream << pipeline.bind_vertices(view).draw(3);
            }
            EXPECT_FALSE(owner.expired());
        }));
        EXPECT_TRUE(owner.expired());
    }

    TEST_F(TypedAHOBufferVulkanIntegrationTest, TextureDescriptorArraysBlockUpdatesInOtherPhasesUntilDrawCompletes) {
        using Textures = t::DescriptorSet<1, t::TextureBinding<2, vsl::ShaderFlag::FRAGMENT, 2>>;
        using TextureSchema = t::ResourceSchema<Textures>;
        aho::x::resource::Texture texture(device, 1, 1);
        Host pixels(device, 4);
        const std::array<std::uint8_t, 4> rgba{64, 128, 191, 255};
        pixels.write(std::as_bytes(std::span(rgba)));
        r::Buffer<r::BufferType::GENERAL> cpu_pixels;
        cpu_pixels.assign(std::as_bytes(std::span(rgba)));
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << texture.update(pixels);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        vsl::Sampler sampler(device, vsl::SamplingMode::NEAREST, vsl::EdgeMode::CLAMP_TO_EDGE);
        vsl::GraphicResourceManager descriptors(device);
        t::TypedDescriptorSet<Textures> set(device, descriptors);
        const auto view = aho::x::resource::typed_texture(texture, sampler);
        set.write<2>(std::array{view, view});
        vsl::Shader<vsl::ShaderType::VERTEX> source(device, vsl_test::shader_path("triangle.vert.spv"));
        vsl::Shader<vsl::ShaderType::FRAGMENT> frag(device, vsl_test::shader_path("typed_texture.frag.spv"));
        namespace pl = vsl::pipeline_layout;
        t::TypedGraphicsPipeline<t::TypedVertexInput<>, TextureSchema> pipeline(device, *pass, pl::ShaderGroup("AHO texture", {source, frag}),
                                                                                pl::ColorBlend{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{}, pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description));
        ExpectTriangleColor(render([&](auto& stream) {
            stream << pipeline.bind_vertices().bind_resources(set).draw(3);
            auto phase = manager.start_phase<vsl::ComputePhase>();
            EXPECT_THROW(phase << texture.update(pixels), std::logic_error);
            EXPECT_THROW(phase << texture.update(cpu_pixels), std::logic_error);
            phase.cancel();
        }));
        auto phase = manager.start_phase<vsl::ComputePhase>();
        EXPECT_NO_THROW(phase << texture.update(cpu_pixels));
        phase.cancel();
    }
} // namespace
#endif
