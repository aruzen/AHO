#include <VSL/utils/typed_pipeline.hpp>
#include <VSL/utils/typed_compute.hpp>
#include "support/render_fixture.hpp"

namespace {
    namespace t = vsl::typed;
    struct Tint {
        float rgba[4];
    };

    using TintRecord = t::BlockRecord<Tint, VSL_BLOCK_MEMBER(Tint, rgba)>;
    using Push = t::PushSchema<t::PushRange<TintRecord, vsl::ShaderFlag::VERTEX, 16>>;
    using Empty = t::TypedVertexInput<>;

    class TypedPushVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }

    protected:
        template<typename Pushes, typename Resources = t::ResourceSchema<>>
        auto make_typed(const char* source = "typed_push.vert.spv", const char* frag = "triangle.frag.spv");
    };
    class TypedPushComputeVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };

    template<typename Pushes, typename Resources>
    auto TypedPushVulkanIntegrationTest::make_typed(const char* source, const char* frag) {
        namespace pl = vsl::pipeline_layout;
        vsl::Shader<vsl::ShaderType::VERTEX> vs(device, vsl_test::shader_path(source));
        vsl::Shader<vsl::ShaderType::FRAGMENT> fs(device, vsl_test::shader_path(frag));
        return t::TypedGraphicsPipeline<Empty, Resources, Pushes>(device, *pass, pl::ShaderGroup("typed pushes", {vs, fs}),
                                                                  pl::ColorBlend{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{},
                                                                  pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description));
    }

    TEST(TypedPushSchemaTest, ValuesOwnTheirSnapshotAndKeepDistinctOffsetsInCpuStorage) {
        using Overlap = t::PushSchema<t::PushRange<TintRecord, vsl::ShaderFlag::VERTEX>, t::PushRange<TintRecord, vsl::ShaderFlag::FRAGMENT, 16>>;
        static_assert(Overlap::DATA_SIZE == 2 * sizeof(Tint));
        static_assert(Overlap::REQUIREMENTS[0].range.offset == 0 && Overlap::REQUIREMENTS[1].range.offset == 16);
        static_assert(Overlap::REQUIREMENTS[1].data_offset == sizeof(Tint));
        static_assert(!std::default_initializable<Push::Values>);
        Tint first{{.25f, 0, .25f, 1}}, second{{0, .5f, .5f, 1}};
        Overlap::Values values(first, second);
        first.rgba[0] = 1;
        Tint restored{};
        std::memcpy(&restored, values.bytes().data(), sizeof(restored));
        EXPECT_EQ(restored.rgba[0], .25f);
        std::memcpy(&restored, values.bytes().data() + sizeof(Tint), sizeof(restored));
        EXPECT_EQ(restored.rgba[1], .5f);
    }

    TEST_F(TypedPushVulkanIntegrationTest, OffsetPushPayloadSurvivesTheOriginalVariablesAndCommandsCanBeReused) {
        auto typed_pipeline = make_typed<Push>();
        auto command = [&] {
            Tint value{{.25f, .5f, .75f, 1}};
            auto bound = typed_pipeline.bind_vertices().push_constants(Push::Values(value));
            value = Tint{};
            return bound.draw(3);
        }();
        ExpectTriangleColor(render([&](auto& stream) { stream << command; }));
        ExpectTriangleColor(render([&](auto& stream) { stream << command; }));
    }

    TEST_F(TypedPushVulkanIntegrationTest, SharedOffsetRequiresEqualBytesAndUpdatesEveryOverlappingStage) {
        using Overlap = t::PushSchema<t::PushRange<TintRecord, vsl::ShaderFlag::VERTEX>, t::PushRange<TintRecord, vsl::ShaderFlag::FRAGMENT>>;
        auto typed_pipeline = make_typed<Overlap>("typed_push_overlap.vert.spv", "typed_push_overlap.frag.spv");
        const Tint shared{{.125f, .25f, .375f, 1}}, conflicting{{0, 0, 0, 1}};
        EXPECT_THROW((Overlap::Values(shared, conflicting)), std::invalid_argument);
        auto bound = typed_pipeline.bind_vertices().push_constants(Overlap::Values(shared, shared));
        ExpectTriangleColor(render([&](auto& stream) { stream << bound.draw(3); }));
    }

    TEST_F(TypedPushVulkanIntegrationTest, DistinctStageOffsetsKeepIndependentValues) {
        using Pushes = t::PushSchema<t::PushRange<TintRecord, vsl::ShaderFlag::VERTEX>, t::PushRange<TintRecord, vsl::ShaderFlag::FRAGMENT, 16>>;
        auto typed_pipeline = make_typed<Pushes>("typed_push_overlap.vert.spv", "typed_push_distinct.frag.spv");
        const Tint vertex{{.25f, 0, .25f, 1}}, fragment{{0, .5f, .5f, 1}};
        auto bound = typed_pipeline.bind_vertices().push_constants(Pushes::Values(vertex, fragment));
        ExpectTriangleColor(render([&](auto& stream) { stream << bound.draw(3); }));
    }

    TEST_F(TypedPushVulkanIntegrationTest, PartialStageOverlapSplitsUpdatesAtNativeRangeBoundaries) {
        struct Partial {
            float xy[2];
        };
        using PartialRecord = t::BlockRecord<Partial, VSL_BLOCK_MEMBER(Partial, xy)>;
        using Pushes = t::PushSchema<t::PushRange<TintRecord, vsl::ShaderFlag::VERTEX>, t::PushRange<PartialRecord, vsl::ShaderFlag::FRAGMENT, 8>>;
        auto typed_pipeline = make_typed<Pushes>("typed_push_overlap.vert.spv", "typed_push_partial.frag.spv");
        const Tint vertex{{.25f, .5f, .25f, 1}};
        const Partial fragment{{.25f, 1}};
        auto bound = typed_pipeline.bind_vertices().push_constants(Pushes::Values(vertex, fragment));
        ExpectTriangleColor(render([&](auto& stream) { stream << bound.draw(3); }));
    }

    TEST_F(TypedPushVulkanIntegrationTest, ResourceAndPushBindingCanBeAppliedInEitherOrder) {
        using Bias = t::DescriptorSet<1, t::UniformBinding<4, TintRecord, vsl::ShaderFlag::VERTEX>>;
        using Resources = t::ResourceSchema<Bias>;
        auto typed_pipeline = make_typed<Push, Resources>("typed_push_resources.vert.spv");
        const Tint zero{}, color{{.25f, .5f, .75f, 1}};
        t::TypedUniformBuffer<Tint> buffer(device, zero);
        vsl::GraphicResourceManager manager(device);
        t::TypedDescriptorSet<Bias::Layout> bias(device, manager);
        bias.write<4>(buffer);
        auto first = typed_pipeline.bind_vertices().push_constants(Push::Values(color)).bind_resources(bias);
        auto second = typed_pipeline.bind_vertices().bind_resources(bias).push_constants(Push::Values(color));
        ExpectTriangleColor(render([&](auto& stream) { stream << first.draw(3); }));
        ExpectTriangleColor(render([&](auto& stream) { stream << second.draw(3); }));
    }

    TEST_F(TypedPushVulkanIntegrationTest, ShaderPushStageOffsetAndExtentAreValidatedBeforeGraphicsPipelineCreation) {
        using WrongStage = t::PushSchema<t::PushRange<TintRecord, vsl::ShaderFlag::FRAGMENT, 16>>;
        using WrongOffset = t::PushSchema<t::PushRange<TintRecord, vsl::ShaderFlag::VERTEX>>;
        using TooSmall = t::PushSchema<t::PushRange<std::uint32_t, vsl::ShaderFlag::VERTEX, 16>>;
        EXPECT_THROW(make_typed<WrongStage>(), std::invalid_argument);
        EXPECT_THROW(make_typed<WrongOffset>(), std::invalid_argument);
        EXPECT_THROW(make_typed<TooSmall>(), std::invalid_argument);
        EXPECT_THROW(make_typed<t::PushSchema<>>(), std::invalid_argument);
        EXPECT_FALSE(has_validation_errors());
    }

    TEST_F(TypedPushComputeVulkanIntegrationTest, DispatchOwnsItsPushValueAfterTheSourceDies) {
        struct Base {
            std::uint32_t value;
        };
        using BaseRecord = t::BlockRecord<Base, VSL_BLOCK_MEMBER(Base, value)>;
        using Pushes = t::PushSchema<t::PushRange<BaseRecord, vsl::ShaderFlag::COMPUTE>>;
        using Output = t::DescriptorSet<0, t::StorageBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
        using Resources = t::ResourceSchema<Output>;
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("fill.comp.spv"));
        t::TypedComputePipeline<Resources, Pushes> typed_pipeline(device, vsl::pipeline_layout::ShaderGroup("typed compute pushes", {shader}));
        const std::array<std::uint32_t, 16> zero{};
        t::TypedStorageBuffer<std::uint32_t> storage(device, zero);
        vsl::GraphicResourceManager manager(device);
        t::TypedDescriptorSet<Output::Layout> output(device, manager);
        output.write<0>(storage);
        auto command = [&] {
            const Base base{773};
            return typed_pipeline.bind_resources(output).push_constants(Pushes::Values(base)).dispatch(zero.size());
        }();
        auto manager_commands = make_manager();
        {
            auto phase = manager_commands.start_phase<vsl::ComputePhase>();
            phase << command;
            vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
            auto native = manager_commands.get_buffer();
            native.data_->command_buffers[native.get_current_buffer_idx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
        }
        device.data_->device.waitIdle();
        auto mapped = storage.backend().data();
        const auto* actual = static_cast<const std::uint32_t*>(mapped.data);
        for (size_t i = 0; i < zero.size(); ++i)
            EXPECT_EQ(actual[i], 773u + i);
    }
} // namespace
