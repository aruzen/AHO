#ifdef AHO_GPU_TESTS
#include <VSL/lifetime/buffer_access.hpp>
#include <AHO/resource/typed_buffer.hpp>
#include <AHO/resource/arena_buffer.hpp>
#include <AHO/x/resource/typed_texture.hpp>
#include <AHO/command/buffer_upload.hpp>
#include <AHO/x/command/texture_update.hpp>
#include <VSL/utils/typed_compute.hpp>
#include <VSL/utils/typed_pipeline.hpp>
#include <VSL/utils/typed_resource_bundle.hpp>
#include <VSL/vulkan/buffer_view.hpp>
#include <VSL/lifetime/native_recording.hpp>
#include "support/render_fixture.hpp"

namespace {
    namespace r = aho::resource;
    namespace t = vsl::typed;
    using Host = r::Buffer<r::BufferType::GPU_FRIENDLY>;
    using Device = r::Buffer<r::BufferType::ON_GPU>;
    class TypedAHOBufferVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };

    TEST_F(TypedAHOBufferVulkanIntegrationTest, SnapshotIsIndependentOfTheLiveBindingAndDoesNotPinTheNextGeneration) {
        std::optional<r::BufferSnapshot> old;
        std::weak_ptr<vsl::impl::BufferImpl> current;
        {
            Host buffer(device, sizeof(std::uint32_t));
            const std::uint32_t value = 37;
            buffer.write(std::as_bytes(std::span(&value, 1)));
            auto binding = buffer.binding();
            old.emplace(binding.snapshot());
            buffer.reserve(128);
            EXPECT_EQ(old->storage().size(), sizeof(value));
            const auto next = binding.snapshot();
            EXPECT_EQ(next.storage().size(), 128u);
            EXPECT_NE(old->storage().identity(), next.storage().identity());
            current = VSL_NAMESPACE::lifetime::detail::BufferAccess::Owned(buffer.native()).share();
        }
        EXPECT_TRUE(current.expired());
        std::uint32_t result = 0;
        old->read_into(std::as_writable_bytes(std::span(&result, 1)));
        EXPECT_EQ(result, 37u);
        EXPECT_THROW(old->read_into(std::as_writable_bytes(std::span(&result, 1)), SIZE_MAX), std::out_of_range);
        auto moved = std::move(*old);
        EXPECT_THROW(old->read_into(std::as_writable_bytes(std::span(&result, 1))), std::logic_error);
        EXPECT_THROW(old->resource_use(), std::logic_error);
        moved.read_into(std::as_writable_bytes(std::span(&result, 1)));
        EXPECT_EQ(result, 37u);
    }

    TEST_F(TypedAHOBufferVulkanIntegrationTest, NativeCommandsShareStorageStateWithAhoAndTypedEntrances) {
        const std::array<std::uint32_t, 4> values{1, 2, 3, 4};
        Host source(device, sizeof(values)), destination(device, sizeof(values));
        source.write(std::as_bytes(std::span(values)));
        auto view = r::vertex_view<std::uint32_t>(destination);
        EXPECT_EQ(view.resource_use().state.get(), &destination.native().use_state());
        EXPECT_NE(view.resource_use().constraint_state, nullptr);
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << vsl::command::CopyBuffer(&source.native(), &destination.native(), sizeof(values), 0, 0, true);
        EXPECT_TRUE(source.busy());
        EXPECT_TRUE(destination.busy());
        EXPECT_FALSE(destination.wait(0));
        EXPECT_THROW(destination.write({}), std::logic_error);
        EXPECT_THROW(destination.reserve(128), std::logic_error);
        EXPECT_THROW(destination.assign({}), std::logic_error);
        phase.cancel();
        EXPECT_FALSE(destination.busy());
        EXPECT_NO_THROW(destination.write(std::as_bytes(std::span(values))));
    }

    TEST_F(TypedAHOBufferVulkanIntegrationTest, StorageGenerationsStaySeparateAndLogicalRestrictionsRemainAdditional) {
        const std::uint32_t value = 17;
        Host buffer(device, sizeof(value));
        buffer.write(std::as_bytes(std::span(&value, 1)));
        auto old_view = r::uniform_view<std::uint32_t>(buffer);
        auto* old_state = old_view.descriptor().resource_use().state.get();
        buffer.reserve(128);
        auto current_view = r::uniform_view<std::uint32_t>(buffer);
        EXPECT_NE(old_state, current_view.descriptor().resource_use().state.get());
        EXPECT_EQ(current_view.descriptor().resource_use().state.get(), &buffer.native().use_state());
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        old_view.descriptor().resource_use().record(manager.get_buffer());
        EXPECT_TRUE(old_state->busy());
        EXPECT_FALSE(buffer.native().use_state().busy());
        EXPECT_TRUE(buffer.busy());
        EXPECT_THROW(buffer.resize(0), std::logic_error);
        phase.cancel();
        EXPECT_FALSE(buffer.busy());
        EXPECT_NO_THROW(buffer.resize(0));
    }
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
        const vsl::NativeRecordingContext recording(manager.get_buffer());
        vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
        vk::CommandBuffer(recording.native_command_buffer()).pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
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
        const vsl::NativeRecordingContext recording(manager.get_buffer());
        vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
        vk::CommandBuffer(recording.native_command_buffer()).pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        std::array<std::uint32_t, 1> output{};
        frame.buffer<0, 1>().read_into(output);
        EXPECT_EQ(output.front(), value);
        EXPECT_FALSE(input.busy());
        EXPECT_NO_THROW(input.write({}));
    }
    TEST_F(TypedAHOBufferVulkanIntegrationTest, ArenaDescriptorBundleKeepsOldAHOStorageAndItsLogicalUpdateConstraint) {
        using Allocation = t::ResourceAllocationSchema<Schema, t::ExternalAllocation<0, 0>, t::BufferAllocation<0, 1>>;
        auto verify = [&](auto input) {
            const std::uint32_t value = 53;
            input.write(std::as_bytes(std::span(&value, 1)));
            const auto view = r::uniform_view<std::uint32_t>(input);
            input.reserve(64);
            const auto current = input.binding().snapshot();
            auto manager = make_manager().independent(1);
            vsl::GraphicResourceManager descriptors(device);
            t::TypedResourceBundle<Allocation, t::ArenaStorage<>> bundle(manager, descriptors, t::external_resource<0, 0>(view));
            vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_frame.comp.spv"));
            t::TypedComputePipeline<Schema> pipeline(device, vsl::pipeline_layout::ShaderGroup("AHO arena external", {shader}));
            auto phase = manager.start_phase<vsl::ComputePhase>();
            const auto& frame = bundle.get(phase);
            phase << frame.bind_resources(pipeline).dispatch(1);
            EXPECT_TRUE(input.busy());
            EXPECT_FALSE(current.storage().ref().use().busy());
            EXPECT_THROW(input.write({}), std::logic_error);
            EXPECT_THROW(input.reserve(128), std::logic_error);
            const vsl::NativeRecordingContext recording(manager.get_buffer());
            const vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
            vk::CommandBuffer(recording.native_command_buffer()).pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
            std::uint32_t actual = 0;
            frame.buffer<0, 1>().read_into(std::span(&actual, 1));
            EXPECT_EQ(actual, value);
            EXPECT_FALSE(input.busy());
            EXPECT_NO_THROW(input.reserve(128));
        };
        verify(Host(device, sizeof(std::uint32_t)));
        verify(r::make_arena_buffer(device, sizeof(std::uint32_t)));
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
