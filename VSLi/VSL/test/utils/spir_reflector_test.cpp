#include "support/vulkan_fixture.hpp"
#include <VSL/utils/spir_reflector.hpp>
#include <span>

namespace {
    TEST(SpirvReflectorTest, ReflectsBindingsVertexInputsAndPushConstantMembersFromFile) {
        vsl::utils::SPIRVReflector reflector({}, vsl_test::shader_path("reflection.vert.spv"));
        const auto& result = reflector.generated;
        ASSERT_EQ(result.binding_points.size(), 1u);
        const auto& bindings = result.binding_points.at(0);
        ASSERT_EQ(bindings.size(), 1u);
        EXPECT_EQ(bindings[0].binding, 0u);
        EXPECT_EQ(bindings[0].bindingType, vsl::graphic_resource::Type::UniformBuffer);
        EXPECT_EQ(bindings[0].shaderType, vsl::ShaderFlag::Vertex);
        EXPECT_EQ(bindings[0].amount, 1u);
        ASSERT_TRUE(result.vertex_input);
        ASSERT_EQ(result.vertex_input->definitions.size(), 2u);
        EXPECT_EQ(result.vertex_input->definitions[0].layouts[0].format, vsl::data_format::FloatRG);
        EXPECT_EQ(result.vertex_input->definitions[1].layouts[0].format, vsl::data_format::FloatRGB);
        ASSERT_TRUE(result.push_constants);
        ASSERT_EQ(result.push_constants->definitions.size(), 1u);
        EXPECT_EQ(result.push_constants->definitions[0].offset, 0u);
        EXPECT_EQ(result.push_constants->definitions[0].size, 16u);
        EXPECT_EQ(result.push_constants->definitions[0].targetShader, vsl::ShaderFlag::Vertex);
    }
    TEST(SpirvReflectorTest, BinaryConstructorRecognizesComputeResourcesAndStage) {
        const auto bytes = vsl_test::read_binary(vsl_test::shader_path("fill.comp.spv"));
        vsl::utils::SPIRVReflector reflector({}, bytes);
        const auto& binding = reflector.generated.binding_points.at(0).at(0);
        EXPECT_EQ(binding.bindingType, vsl::graphic_resource::Type::StorageBuffer);
        EXPECT_EQ(binding.shaderType, vsl::ShaderFlag::Compute);
        ASSERT_TRUE(reflector.generated.push_constants);
        EXPECT_EQ(reflector.generated.push_constants->definitions[0].size, 4u);
    }
    TEST(SpirvReflectorTest, MissingFileReportsPathLoadingFailure) {
        EXPECT_THROW(vsl::utils::SPIRVReflector({}, vsl_test::shader_path("absent.spv")),
                     vsl::exceptions::RuntimeException);
    }
    class SpirvReflectorVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(SpirvReflectorVulkanIntegrationTest, GeneratedBindingLayoutsRetainDefinitionsAndRepeatCount) {
        vsl::utils::SPIRVReflector reflector(device, vsl_test::shader_path("fill.comp.spv"));
        const auto layouts = reflector.generated.makeBindingLayout(2);
        ASSERT_EQ(layouts.size(), 3u);
        for (const auto& layout : layouts) {
            const auto points = layout.getBindingPoints();
            ASSERT_EQ(points.size(), 1u);
            EXPECT_EQ(points[0], reflector.generated.binding_points.at(0).at(0));
        }
    }
} // namespace

namespace {
    TEST(SpirvReflectorTest, DescriptorArraysUseTheirElementCountsForEverySupportedResourceCategory) {
        vsl::utils::SPIRVReflector reflector({}, vsl_test::shader_path("descriptor_arrays.comp.spv"));
        const auto& points = reflector.generated.binding_points.at(0);
        ASSERT_EQ(points.size(), 4u);
        for (const auto& [binding, type, amount] : {
                 std::tuple{0u, vsl::graphic_resource::Type::UniformBuffer, 2u},
                 std::tuple{1u, vsl::graphic_resource::Type::StorageBuffer, 3u},
                 std::tuple{2u, vsl::graphic_resource::Type::StorageImage, 4u},
                 std::tuple{3u, vsl::graphic_resource::Type::CombinedImageSampler, 5u}}) {
            const auto found = std::ranges::find(points, binding, &vsl::graphic_resource::BindingPoint::binding);
            ASSERT_NE(found, points.end());
            EXPECT_EQ(found->bindingType, type);
            EXPECT_EQ(found->amount, amount);
        }
    }
    TEST(SpirvReflectorTest, MultidimensionalDescriptorArrayCountsMultiplyAllExtents) {
        vsl::utils::SPIRVReflector reflector({}, vsl_test::shader_path("descriptor_multidimensional.comp.spv"));
        const auto& points = reflector.generated.binding_points.at(0);
        const auto found = std::ranges::find(points, 0u, &vsl::graphic_resource::BindingPoint::binding);
        ASSERT_NE(found, points.end());
        EXPECT_EQ(found->amount, 6u);
    }
    TEST(SpirvReflectorTest, UnresolvedAndOverflowingDescriptorCountsReportTheirCause) {
        for (const auto& [file, reason] : {
                 std::pair{"descriptor_runtime.comp.spv", "runtime"},
                 std::pair{"descriptor_count_overflow.comp.spv", "overflow"}}) {
            try {
                vsl::utils::SPIRVReflector reflector({}, vsl_test::shader_path(file));
                FAIL() << file << " cannot be represented as a fixed uint32 descriptor count";
            } catch (const vsl::exceptions::RuntimeException& error) {
                EXPECT_NE(error.reason.find(reason), std::string::npos) << error.reason;
            }
        }
    }
    TEST_F(SpirvReflectorVulkanIntegrationTest, FixedArrayCountsSurviveGeneratedNativeBindingLayouts) {
        vsl::utils::SPIRVReflector reflector(device, vsl_test::shader_path("descriptor_arrays.comp.spv"));
        auto layouts = reflector.generated.makeBindingLayout();
        ASSERT_EQ(layouts.size(), 1u);
        const auto points = layouts.front().getBindingPoints();
        ASSERT_EQ(points.size(), 4u);
        for (const auto& point : points)
            EXPECT_EQ(point.amount, point.binding + 2u);
    }
} // namespace

namespace {
    TEST(SpirvReflectorTest, SpecializationDescriptorCountsUseDefaultsAndEvaluateDefaultExpressions) {
        for (const auto& [file, amount] : {std::pair{"descriptor_specialization.comp.spv", 3u},
                                           std::pair{"descriptor_specialization_expression.comp.spv", 7u}}) {
            vsl::utils::SPIRVReflector reflector({}, vsl_test::shader_path(file));
            const auto& points = reflector.generated.binding_points.at(0);
            const auto found = std::ranges::find(points, 0u, &vsl::graphic_resource::BindingPoint::binding);
            ASSERT_NE(found, points.end());
            EXPECT_EQ(found->amount, amount);
        }
    }
} // namespace

#include "support/temporary_directory.hpp"
#include <fstream>
namespace {
    TEST(SpirvReflectorTest, BinaryRejectsTrailingPartialWordsInsteadOfSilentlyIgnoringThem) {
        const auto valid = vsl_test::read_binary(vsl_test::shader_path("fill.comp.spv"));
        for (std::size_t trailing = 1; trailing <= 3; ++trailing) {
            SCOPED_TRACE(trailing);
            auto bytes = valid;
            bytes.insert(bytes.end(), trailing, '\0');
            try {
                vsl::utils::SPIRVReflector reflector({}, bytes);
                ADD_FAILURE() << "A partial SPIR-V word was accepted";
            } catch (const vsl::exceptions::RuntimeException& error) {
                EXPECT_NE(error.reason.find("size"), std::string::npos);
            }
        }
    }
    TEST(SpirvReflectorTest, BinaryRejectsEmptyOrTruncatedHeadersWithVslRuntimeException) {
        const auto valid = vsl_test::read_binary(vsl_test::shader_path("fill.comp.spv"));
        for (std::size_t length = 0; length < 20; length += 4) {
            SCOPED_TRACE(length);
            const std::vector<char> bytes(valid.begin(), valid.begin() + length);
            EXPECT_THROW(vsl::utils::SPIRVReflector({}, bytes), vsl::exceptions::RuntimeException);
        }
    }
    TEST(SpirvReflectorTest, FileRejectsTrailingPartialWordsInsteadOfSilentlyIgnoringThem) {
        vsl_test::TemporaryDirectory directory;
        const auto path = directory.path() / "partial.spv";
        const auto valid = vsl_test::read_binary(vsl_test::shader_path("fill.comp.spv"));
        for (std::size_t trailing = 1; trailing <= 3; ++trailing) {
            SCOPED_TRACE(trailing);
            auto bytes = valid;
            bytes.insert(bytes.end(), trailing, '\0');
            {
                std::ofstream output(path, std::ios::binary);
                ASSERT_TRUE(output.write(bytes.data(), static_cast<std::streamsize>(bytes.size())));
            }
            EXPECT_THROW(vsl::utils::SPIRVReflector({}, path), vsl::exceptions::RuntimeException);
        }
    }
    TEST(SpirvReflectorTest, FileRejectsEmptyOrTruncatedHeadersWithVslRuntimeException) {
        vsl_test::TemporaryDirectory directory;
        const auto path = directory.path() / "short.spv";
        const auto valid = vsl_test::read_binary(vsl_test::shader_path("fill.comp.spv"));
        for (std::size_t length = 0; length < 20; length += 4) {
            SCOPED_TRACE(length);
            {
                std::ofstream output(path, std::ios::binary);
                ASSERT_TRUE(output.write(valid.data(), static_cast<std::streamsize>(length)));
            }
            EXPECT_THROW(vsl::utils::SPIRVReflector({}, path), vsl::exceptions::RuntimeException);
        }
    }
    TEST(SpirvReflectorTest, FileAndBinaryConstructorsPreserveValidReflectionFromTemporaryPath) {
        vsl_test::TemporaryDirectory directory;
        const auto path = directory.path() / "反射 shader.spv";
        const auto bytes = vsl_test::read_binary(vsl_test::shader_path("fill.comp.spv"));
        {
            std::ofstream output(path, std::ios::binary);
            ASSERT_TRUE(output.write(bytes.data(), static_cast<std::streamsize>(bytes.size())));
        }
        vsl::utils::SPIRVReflector from_file({}, path), from_binary({}, bytes);
        EXPECT_EQ(from_file.generated.binding_points, from_binary.generated.binding_points);
        ASSERT_TRUE(from_file.generated.push_constants);
        EXPECT_EQ(from_file.generated.push_constants->definitions.at(0).size, 4u);
        EXPECT_EQ(from_file.generated.push_constants->definitions.at(0).targetShader, vsl::ShaderFlag::Compute);
    }
    TEST(SpirvReflectorTest, BinaryPreservesSupportForSwappedWordByteOrder) {
        auto bytes = vsl_test::read_binary(vsl_test::shader_path("fill.comp.spv"));
        ASSERT_EQ(bytes.size() % 4, 0u);
        for (std::size_t i = 0; i < bytes.size(); i += 4)
            std::reverse(bytes.begin() + i, bytes.begin() + i + 4);
        vsl::utils::SPIRVReflector reflector({}, bytes);
        const auto& binding = reflector.generated.binding_points.at(0).at(0);
        EXPECT_EQ(binding.bindingType, vsl::graphic_resource::Type::StorageBuffer);
        EXPECT_EQ(binding.shaderType, vsl::ShaderFlag::Compute);
        ASSERT_TRUE(reflector.generated.push_constants);
        EXPECT_EQ(reflector.generated.push_constants->definitions.at(0).size, 4u);
    }
    TEST(SpirvReflectorTest, MissingFileFailureIncludesTheRequestedPath) {
        vsl_test::TemporaryDirectory directory;
        const auto path = directory.path() / "missing shader.spv";
        try {
            vsl::utils::SPIRVReflector reflector({}, path);
            ADD_FAILURE() << "Missing SPIR-V file was accepted";
        } catch (const vsl::exceptions::RuntimeException& error) {
            EXPECT_NE(error.reason.find(path.string()), std::string::npos);
        }
    }
} // namespace

namespace {
    TEST(SpirvReflectorTest, DenseVertexInputsRetainExplicitShaderLocations) {
        vsl::utils::SPIRVReflector reflector({}, vsl_test::shader_path("reflection.vert.spv"));
        ASSERT_TRUE(reflector.generated.vertex_input);
        const auto& definitions = reflector.generated.vertex_input->definitions;
        ASSERT_EQ(definitions.size(), 2u);
        EXPECT_EQ(definitions[0].layouts.at(0).location, 0u);
        EXPECT_EQ(definitions[1].layouts.at(0).location, 1u);
    }
    TEST(SpirvReflectorTest, SparseVertexInputsRetainLocationsWithoutRenumbering) {
        vsl::utils::SPIRVReflector reflector({}, vsl_test::shader_path("reflection_sparse.vert.spv"));
        ASSERT_TRUE(reflector.generated.vertex_input);
        const auto& definitions = reflector.generated.vertex_input->definitions;
        ASSERT_EQ(definitions.size(), 2u);
        ASSERT_EQ(definitions[0].layouts.size(), 1u);
        ASSERT_EQ(definitions[1].layouts.size(), 1u);
        EXPECT_EQ(definitions[0].layouts[0].location, 3u);
        EXPECT_EQ(definitions[0].layouts[0].format, vsl::data_format::FloatRG);
        EXPECT_EQ(definitions[1].layouts[0].location, 7u);
        EXPECT_EQ(definitions[1].layouts[0].format, vsl::data_format::FloatRGB);
    }
    TEST(SpirvReflectorTest, MatrixColumnsStartAtTheDeclaredLocation) {
        vsl::utils::SPIRVReflector reflector({}, vsl_test::shader_path("reflection_matrix.vert.spv"));
        ASSERT_TRUE(reflector.generated.vertex_input);
        const auto& definitions = reflector.generated.vertex_input->definitions;
        ASSERT_EQ(definitions.size(), 2u);
        const auto matrix = std::ranges::find_if(definitions, [](const auto& def) { return def.layouts.size() == 3; });
        ASSERT_NE(matrix, definitions.end());
        for (std::size_t column = 0; column < 3; ++column) {
            EXPECT_EQ(matrix->layouts[column].location, 4u + column);
            EXPECT_EQ(matrix->layouts[column].format, vsl::data_format::FloatRGB);
        }
        const auto position = std::ranges::find_if(definitions, [](const auto& def) { return def.layouts.size() == 1; });
        ASSERT_NE(position, definitions.end());
        EXPECT_EQ(position->layouts[0].location, 1u);
        EXPECT_EQ(position->layouts[0].format, vsl::data_format::FloatRG);
    }
    class ReflectedLocationsVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(ReflectedLocationsVulkanIntegrationTest, InjectionPreservesSparseLocationsAndMatrixColumnOffsets) {
        for (const auto& [file, expected_locations] : {
                 std::pair{"reflection_sparse.vert.spv", std::vector<std::uint32_t>{3, 7}},
                 std::pair{"reflection_matrix.vert.spv", std::vector<std::uint32_t>{1, 4, 5, 6}}}) {
            SCOPED_TRACE(file);
            vsl::utils::SPIRVReflector reflector(device, vsl_test::shader_path(file));
            ASSERT_TRUE(reflector.generated.vertex_input);
            auto layout = make_layout_state();
            reflector.generated.vertex_input->injection(layout);
            const auto& native = layout._data->info->vertexInput;
            ASSERT_EQ(native.vertexAttributeDescriptionCount, expected_locations.size());
            std::vector<std::uint32_t> locations;
            for (std::uint32_t i = 0; i < native.vertexAttributeDescriptionCount; ++i) {
                const auto& attribute = native.pVertexAttributeDescriptions[i];
                locations.push_back(attribute.location);
                if (attribute.format == vk::Format::eR32G32B32Sfloat && expected_locations.size() == 4) {
                    EXPECT_EQ(attribute.offset, (attribute.location - 4) * 12);
                    const auto bindings = std::span(native.pVertexBindingDescriptions, native.vertexBindingDescriptionCount);
                    const auto binding = std::ranges::find(bindings, attribute.binding, &vk::VertexInputBindingDescription::binding);
                    ASSERT_NE(binding, bindings.end());
                    EXPECT_EQ(binding->stride, 36u);
                }
            }
            std::ranges::sort(locations);
            EXPECT_EQ(locations, expected_locations);
        }
    }
} // namespace

#include "support/render_fixture.hpp"
namespace {
    class ReflectedLocationsRenderVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(ReflectedLocationsRenderVulkanIntegrationTest, ReflectedSparseVertexLocationsProduceExpectedGpuColor) {
        vsl::utils::SPIRVReflector reflector(device, vsl_test::shader_path("reflection_sparse.vert.spv"));
        ASSERT_TRUE(reflector.generated.vertex_input);
        const auto& definitions = reflector.generated.vertex_input->definitions;
        ASSERT_EQ(definitions.size(), 2u);
        // Reject incorrect metadata before constructing a native pipeline with mismatched shader inputs.
        ASSERT_EQ(definitions[0].layouts.at(0).location, 3u);
        ASSERT_EQ(definitions[1].layouts.at(0).location, 7u);
        const std::array<std::array<float, 2>, 3> positions{{{-0.75f, -0.75f}, {0.75f, -0.75f}, {0, 0.75f}}};
        const std::array<std::array<float, 3>, 3> colors{{{0.25f, 0.5f, 0.75f}, {0.25f, 0.5f, 0.75f}, {0.25f, 0.5f, 0.75f}}};
        vsl::StagingBuffer<vsl::MemoryType::VertexBuffer> position_buffer(device, positions), color_buffer(device, colors);
        pipeline.reset();
        layout.reset();
        vertex.emplace(device, vsl_test::shader_path("reflection_sparse.vert.spv"));
        namespace pl = vsl::pipeline_layout;
        layout.emplace(device, pl::ShaderGroup("reflected sparse inputs", {*vertex, *fragment}), *reflector.generated.vertex_input,
                       pl::ColorBlend{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::None}, pl::Multisample{},
                       pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description));
        pipeline.emplace(*layout, *pass);
        expect_triangle_color(render([&](vsl::DefaultPhaseStream& stream) {
            stream << vsl::command::BindVertexBuffer(position_buffer, color_buffer) << vsl::command::Draw(3);
        }));
    }
} // namespace

#include <limits>
namespace {
    TEST(SpirvReflectorTest, RepeatedLayoutCountOverflowIsRejectedBeforeNativeAllocation) {
        for (const auto set_count : {1u, 2u}) {
            SCOPED_TRACE(set_count);
            vsl::utils::SPIRVReflector reflector({}, vsl_test::shader_path("fill.comp.spv"));
            if (set_count == 2)
                reflector.generated.binding_points[1] = reflector.generated.binding_points.at(0);
            const auto before = reflector.generated.binding_points;
            try {
                (void)reflector.generated.makeBindingLayout(std::numeric_limits<std::size_t>::max());
                ADD_FAILURE() << "An overflowing repeat count silently returned an empty layout list";
            } catch (const vsl::exceptions::RuntimeException& error) {
                EXPECT_NE(error.reason.find("repeat"), std::string::npos);
            }
            EXPECT_EQ(reflector.generated.binding_points, before);
        }
    }
    TEST(SpirvReflectorTest, EmptyBindingDefinitionsNeedNoNativeAllocationRegardlessOfRepeatCount) {
        vsl::utils::SPIRVReflector::GenerateData data{};
        EXPECT_TRUE(data.makeBindingLayout().empty());
        EXPECT_TRUE(data.makeBindingLayout(std::numeric_limits<std::size_t>::max()).empty());
        EXPECT_TRUE(data.binding_points.empty());
    }
} // namespace

namespace {
    TEST(SpirvReflectorTest, SparseSetMetadataPreservesTheShaderDecoration) {
        vsl::utils::SPIRVReflector reflector({}, vsl_test::shader_path("reflection_sparse_sets.comp.spv"));
        ASSERT_EQ(reflector.generated.binding_points.size(), 1u);
        ASSERT_TRUE(reflector.generated.binding_points.contains(2));
        const auto& points = reflector.generated.binding_points.at(2);
        ASSERT_EQ(points.size(), 1u);
        EXPECT_EQ(points[0].binding, 0u);
        EXPECT_EQ(points[0].bindingType, vsl::graphic_resource::Type::StorageBuffer);
        EXPECT_EQ(points[0].shaderType, vsl::ShaderFlag::Compute);
    }
    class ReflectedSetSlotsVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(ReflectedSetSlotsVulkanIntegrationTest, MissingSetsProduceEmptyLayoutsAtTheirOriginalIndices) {
        vsl::utils::SPIRVReflector reflector(device, vsl_test::shader_path("reflection_sparse_sets.comp.spv"));
        const auto before = reflector.generated.binding_points;
        const auto layouts = reflector.generated.makeBindingLayout();
        ASSERT_EQ(layouts.size(), 3u);
        EXPECT_TRUE(layouts[0].getBindingPoints().empty());
        EXPECT_TRUE(layouts[1].getBindingPoints().empty());
        EXPECT_EQ(layouts[2].getBindingPoints(), before.at(2));
        EXPECT_EQ(reflector.generated.binding_points, before);
        auto layout = make_layout_state();
        vsl::pipeline_layout::ResourceBinding(layouts).injection(layout);
        ASSERT_EQ(layout._data->info->pipelineLayout.setLayoutCount, 3u);
        for (std::size_t set = 0; set < layouts.size(); ++set)
            EXPECT_EQ(layout._data->info->pipelineLayout.pSetLayouts[set], layouts[set]._data->layout);
    }
    TEST_F(ReflectedSetSlotsVulkanIntegrationTest, RepeatedSparseSetsKeepTheFullSequenceAndIndependentOwners) {
        vsl::utils::SPIRVReflector reflector(device, vsl_test::shader_path("reflection_sparse_sets.comp.spv"));
        const auto layouts = reflector.generated.makeBindingLayout(2);
        ASSERT_EQ(layouts.size(), 9u);
        for (std::size_t block = 0; block < 3; ++block) {
            EXPECT_TRUE(layouts[block * 3].getBindingPoints().empty());
            EXPECT_TRUE(layouts[block * 3 + 1].getBindingPoints().empty());
            EXPECT_EQ(layouts[block * 3 + 2].getBindingPoints(), reflector.generated.binding_points.at(2));
            if (block)
                EXPECT_NE(layouts[block * 3 + 2]._data, layouts[2]._data);
        }
    }
    TEST_F(ReflectedSetSlotsVulkanIntegrationTest, MultipleSparseSetsKeepTheirSetNumbers) {
        vsl::utils::SPIRVReflector reflector(device, vsl_test::shader_path("reflection_sparse_sets.comp.spv"));
        reflector.generated.binding_points[0] = reflector.generated.binding_points.at(2);
        const auto layouts = reflector.generated.makeBindingLayout();
        ASSERT_EQ(layouts.size(), 3u);
        EXPECT_EQ(layouts[0].getBindingPoints(), reflector.generated.binding_points.at(0));
        EXPECT_TRUE(layouts[1].getBindingPoints().empty());
        EXPECT_EQ(layouts[2].getBindingPoints(), reflector.generated.binding_points.at(2));
    }
    TEST_F(ReflectedSetSlotsVulkanIntegrationTest, InvalidSetNumbersAreRejectedBeforeAllocatingNativeLayouts) {
        const auto limit = physical._data->props->limits.maxBoundDescriptorSets;
        for (const auto set : {std::numeric_limits<std::size_t>::max(), std::size_t{limit}}) {
            SCOPED_TRACE(set);
            vsl::utils::SPIRVReflector::GenerateData data{.logical_device = device};
            data.binding_points[set] = {{0, vsl::graphic_resource::Type::StorageBuffer, vsl::ShaderFlag::Compute, 1}};
            const auto before = data.binding_points;
            try {
                (void)data.makeBindingLayout();
                ADD_FAILURE() << "An unusable set number was accepted";
            } catch (const vsl::exceptions::RuntimeException& error) {
                EXPECT_NE(error.reason.find("set"), std::string::npos);
            }
            EXPECT_EQ(data.binding_points, before);
        }
    }
} // namespace

#include "support/compute_fixture.hpp"
namespace {
    class ReflectedSetComputeVulkanIntegrationTest : public vsl_test::ComputeVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(ReflectedSetComputeVulkanIntegrationTest, GeneratedSparseLayoutsSupportShaderWritesAtSetTwo) {
        vsl::utils::SPIRVReflector reflector(device, vsl_test::shader_path("reflection_sparse_sets.comp.spv"));
        const auto sets = reflector.generated.makeBindingLayout();
        ASSERT_EQ(sets.size(), 3u) << "Do not create a pipeline with incorrectly numbered reflected sets";
        auto [success, allocated] = pool.bind(sets);
        ASSERT_TRUE(success);
        ASSERT_EQ(allocated.size(), 3u);
        allocated[2].update(&*output);
        vsl::Shader<vsl::ShaderType::Compute> reflected_shader(device, vsl_test::shader_path("reflection_sparse_sets.comp.spv"));
        vsl::PipelineLayout reflected_layout(device, vsl::pipeline_layout::ShaderGroup("reflected set two", {reflected_shader}),
                                             vsl::pipeline_layout::ResourceBinding(sets));
        vsl::ComputePipeline reflected_pipeline(reflected_layout);
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << reflected_pipeline << vsl::command::BindGraphicResource(allocated, vsl::graphic_resource::BindingDestination::Compute, reflected_pipeline) << vsl::command::Dispatch(count);
            vk::MemoryBarrier barrier{};
            barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
            auto commands = manager.getBuffer();
            commands._data->commandBuffers[commands.getCurrentBufferIdx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
        }
        device._data->device.waitIdle();
        auto mapped = output->data(count * sizeof(std::uint32_t));
        std::array<std::uint32_t, count> actual{};
        std::memcpy(actual.data(), mapped.data, sizeof(actual));
        expect_sequence(actual, 37);
    }
} // namespace

namespace {
    TEST(SpirvReflectorTest, NumericVertexFormatsKeepTheirSignednessAnd16Or64BitWidth) {
        vsl::utils::SPIRVReflector reflector({}, vsl_test::shader_path("reflection_numeric_widths.vert.spv"));
        ASSERT_TRUE(reflector.generated.vertex_input);
        const auto& definitions = reflector.generated.vertex_input->definitions;
        ASSERT_EQ(definitions.size(), 9u);
        for (const auto& [location, format, bytes] : {
                 std::tuple{0u, vsl::data_format::SignedFloat16RG, 4u},
                 std::tuple{1u, vsl::data_format::SignedInt16RGB, 6u},
                 std::tuple{2u, vsl::data_format::UnsignedInt16RGBA, 8u},
                 std::tuple{3u, vsl::data_format::SignedFloat64RGB, 24u},
                 std::tuple{11u, vsl::data_format::SignedInt64RG, 16u},
                 std::tuple{12u, vsl::data_format::UnsignedInt64RG, 16u},
                 std::tuple{15u, vsl::data_format::SignedFloat64RGBA, 32u}}) {
            SCOPED_TRACE(location);
            const auto found = std::ranges::find_if(definitions, [&](const auto& def) {
                return def.layouts.size() == 1 && def.layouts[0].location == location;
            });
            ASSERT_NE(found, definitions.end());
            EXPECT_EQ(found->layouts[0].format, format);
            EXPECT_EQ(found->layouts[0].format.size(), bytes);
        }
    }
    TEST(SpirvReflectorTest, DoubleMatrixColumnsUseTheLocationsRequiredByTheirVectorWidth) {
        vsl::utils::SPIRVReflector reflector({}, vsl_test::shader_path("reflection_numeric_widths.vert.spv"));
        ASSERT_TRUE(reflector.generated.vertex_input);
        const auto& definitions = reflector.generated.vertex_input->definitions;
        const auto matrix = std::ranges::find_if(definitions, [](const auto& def) { return def.layouts.size() == 3; });
        ASSERT_NE(matrix, definitions.end());
        for (std::uint32_t column = 0; column < 3; ++column) {
            EXPECT_EQ(matrix->layouts[column].location, 5u + column * 2);
            EXPECT_EQ(matrix->layouts[column].format, vsl::data_format::SignedFloat64RGB);
            EXPECT_EQ(matrix->layouts[column].format.size(), 24u);
        }
        const auto compact = std::ranges::find_if(definitions, [](const auto& def) { return def.layouts.size() == 2; });
        ASSERT_NE(compact, definitions.end());
        for (std::uint32_t column = 0; column < 2; ++column) {
            EXPECT_EQ(compact->layouts[column].location, 13u + column);
            EXPECT_EQ(compact->layouts[column].format, vsl::data_format::SignedFloat64RG);
        }
    }
    TEST(SpirvReflectorTest, ReflectedNumericWidthRequirementsIncludeEveryMatrixColumn) {
        vsl::utils::SPIRVReflector reflector({}, vsl_test::shader_path("reflection_numeric_widths.vert.spv"));
        ASSERT_TRUE(reflector.generated.vertex_input);
        // Separate bindings: half/short vectors, dvec3/4, dmat3/2 and signed/unsigned int64 vectors.
        EXPECT_EQ(reflector.generated.vertex_input->requirements_size(), 4u + 6u + 8u + 24u + 72u + 16u + 16u + 32u + 32u);
    }
    class ReflectedNumericFormatsVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(ReflectedNumericFormatsVulkanIntegrationTest, NativeInputMetadataKeepsFormatsWidthsAndMatrixColumnOffsets) {
        vsl::utils::SPIRVReflector reflector(device, vsl_test::shader_path("reflection_numeric_widths.vert.spv"));
        ASSERT_TRUE(reflector.generated.vertex_input);
        auto layout = make_layout_state();
        reflector.generated.vertex_input->injection(layout);
        const auto& native = layout._data->info->vertexInput;
        ASSERT_EQ(native.vertexBindingDescriptionCount, 9u);
        ASSERT_EQ(native.vertexAttributeDescriptionCount, 12u);
        const auto attributes = std::span(native.pVertexAttributeDescriptions, native.vertexAttributeDescriptionCount);
        for (const auto& [location, format] : {
                 std::pair{0u, vk::Format::eR16G16Sfloat},
                 std::pair{1u, vk::Format::eR16G16B16Sint},
                 std::pair{2u, vk::Format::eR16G16B16A16Uint},
                 std::pair{3u, vk::Format::eR64G64B64Sfloat},
                 std::pair{5u, vk::Format::eR64G64B64Sfloat},
                 std::pair{7u, vk::Format::eR64G64B64Sfloat},
                 std::pair{9u, vk::Format::eR64G64B64Sfloat},
                 std::pair{11u, vk::Format::eR64G64Sint},
                 std::pair{12u, vk::Format::eR64G64Uint},
                 std::pair{13u, vk::Format::eR64G64Sfloat},
                 std::pair{14u, vk::Format::eR64G64Sfloat},
                 std::pair{15u, vk::Format::eR64G64B64A64Sfloat}}) {
            SCOPED_TRACE(location);
            const auto attribute = std::ranges::find(attributes, location, &vk::VertexInputAttributeDescription::location);
            ASSERT_NE(attribute, attributes.end());
            EXPECT_EQ(attribute->format, format);
            if (location == 5 || location == 7 || location == 9) {
                EXPECT_EQ(attribute->offset, (location - 5) / 2 * 24);
                const auto bindings = std::span(native.pVertexBindingDescriptions, native.vertexBindingDescriptionCount);
                const auto binding = std::ranges::find(bindings, attribute->binding, &vk::VertexInputBindingDescription::binding);
                ASSERT_NE(binding, bindings.end());
                EXPECT_EQ(binding->stride, 72u);
            } else if (location == 13 || location == 14) {
                EXPECT_EQ(attribute->offset, (location - 13) * 16);
                const auto bindings = std::span(native.pVertexBindingDescriptions, native.vertexBindingDescriptionCount);
                const auto binding = std::ranges::find(bindings, attribute->binding, &vk::VertexInputBindingDescription::binding);
                ASSERT_NE(binding, bindings.end());
                EXPECT_EQ(binding->stride, 32u);
            }
        }
        // The metadata is independent of device shaderFloat64 / storageInputOutput16 support;
        // this test does not create a pipeline with unsupported shader features.
    }
} // namespace
