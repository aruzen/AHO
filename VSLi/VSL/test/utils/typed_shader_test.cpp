#include <VSL/utils/typed_shader.hpp>
#include "support/vulkan_fixture.hpp"

namespace {
    namespace t = vsl::typed;
    namespace f = vsl::data_format;
    using Reflector = vsl::utils::SPIRVReflector;
    template<typename... Attributes>
    using Input = t::TypedVertexInput<t::VertexShapeDefinition<0, t::AUTO, t::VertexTiming::NEXT_VERTEX, Attributes...>>;
    using Ordinary = Input<t::VertexInputDefinition<f::FLOAT_RG, 0>, t::VertexInputDefinition<f::FLOAT_RGB, 1>>;

    TEST(TypedShaderInputTest, MissingAttributesReportEntryPointNameAndLocation) {
        Reflector shader({}, vsl_test::shader_path("vertices.vert.spv"));
        EXPECT_NO_THROW(t::validate_vertex_shader<Ordinary>(shader.generated));
        using MissingColor = Input<t::VertexInputDefinition<f::FLOAT_RG, 0>>;
        try {
            t::validate_vertex_shader<MissingColor>(shader.generated);
            FAIL() << "Missing color attribute was accepted";
        } catch (const std::invalid_argument& error) {
            const std::string message = error.what();
            EXPECT_NE(message.find("main"), std::string::npos);
            EXPECT_NE(message.find("input_color"), std::string::npos);
            EXPECT_NE(message.find("location 1"), std::string::npos);
            EXPECT_NE(message.find("missing"), std::string::npos);
        }
    }

    TEST(TypedShaderInputTest, FloatIntegerSignednessAnd64BitWidthAreChecked) {
        Reflector shader({}, vsl_test::shader_path("vertices.vert.spv"));
        using Integer = Input<t::VertexInputDefinition<f::INT_RG, 0>, t::VertexInputDefinition<f::FLOAT_RGB, 1>>;
        using Wide = Input<t::VertexInputDefinition<f::FLOAT64_RG, 0>, t::VertexInputDefinition<f::FLOAT_RGB, 1>>;
        EXPECT_THROW(t::validate_vertex_shader<Integer>(shader.generated), std::invalid_argument);
        EXPECT_THROW(t::validate_vertex_shader<Wide>(shader.generated), std::invalid_argument);
        Reflector numeric({}, vsl_test::shader_path("typed_numeric.vert.spv"));
        using Correct = Input<t::VertexInputDefinition<f::INT16_RG, 0>, t::VertexInputDefinition<f::UNSIGNED_INT8_RGB, 1>>;
        using WrongSign = Input<t::VertexInputDefinition<f::UNSIGNED_INT_RG, 0>, t::VertexInputDefinition<f::UNSIGNED_INT_RGB, 1>>;
        EXPECT_NO_THROW(t::validate_vertex_shader<Correct>(numeric.generated));
        EXPECT_THROW(t::validate_vertex_shader<WrongSign>(numeric.generated), std::invalid_argument);
    }

    TEST(TypedShaderInputTest, NormalizedAndScaledStorageAndDefaultComponentsRemainValid) {
        Reflector shader({}, vsl_test::shader_path("vertices.vert.spv"));
        using Normalized = Input<t::VertexInputDefinition<f::FLOAT16_RG, 0>, t::VertexInputDefinition<f::UNSIGNED_NORMALIZED8_RGBA, 1>>;
        using Scaled = Input<t::VertexInputDefinition<f::SIGNED_SCALED16_RG, 0>, t::VertexInputDefinition<f::FLOAT, 1>>;
        EXPECT_NO_THROW(t::validate_vertex_shader<Normalized>(shader.generated));
        EXPECT_NO_THROW(t::validate_vertex_shader<Scaled>(shader.generated));
    }

    TEST(TypedShaderInputTest, ComponentDecorationsShareOneAttributeAndUnusedInputsAreExcluded) {
        Reflector shader({}, vsl_test::shader_path("typed_components.vert.spv"));
        ASSERT_EQ(shader.generated.vertex_inputs.size(), 3u);
        using Packed = Input<t::VertexInputDefinition<f::FLOAT_RGBA, 0>>;
        EXPECT_NO_THROW(t::validate_vertex_shader<Packed>(shader.generated));
        for (const auto& variable : shader.generated.vertex_inputs) EXPECT_EQ(variable.location, 0u);
    }

    TEST(TypedShaderInputTest, WideComponentsMayUseTheSecondLocationButNeverMissingDefaults) {
        Reflector shader({}, vsl_test::shader_path("typed_wide.vert.spv"));
        using Full = Input<t::VertexInputDefinition<f::FLOAT64_RGBA, 2>>;
        using Short = Input<t::VertexInputDefinition<f::FLOAT64_RGB, 2>>;
        EXPECT_NO_THROW(t::validate_vertex_shader<Full>(shader.generated));
        EXPECT_THROW(t::validate_vertex_shader<Short>(shader.generated), std::invalid_argument);
    }

    TEST(TypedShaderInputTest, ArraysAndMatricesExpandIntoTheirDeclaredLocations) {
        Reflector shader({}, vsl_test::shader_path("reflection_vertex_array_shapes.vert.spv"));
        ASSERT_EQ(shader.generated.vertex_inputs.size(), 12u);
        using MatrixArrays = Input<
            t::VertexInputDefinition<f::FLOAT_RGB, 2>, t::VertexInputDefinition<f::FLOAT_RGB, 3>,
            t::VertexInputDefinition<f::FLOAT_RGB, 4>, t::VertexInputDefinition<f::FLOAT_RGB, 5>,
            t::VertexInputDefinition<f::FLOAT64_RGB, 7>, t::VertexInputDefinition<f::FLOAT64_RGB, 9>,
            t::VertexInputDefinition<f::FLOAT64_RGB, 11>, t::VertexInputDefinition<f::FLOAT64_RGB, 13>,
            t::VertexInputDefinition<f::FLOAT_RG, 20>, t::VertexInputDefinition<f::FLOAT_RG, 21>,
            t::VertexInputDefinition<f::FLOAT_RG, 22>, t::VertexInputDefinition<f::FLOAT_RG, 23>>;
        EXPECT_NO_THROW(t::validate_vertex_shader<MatrixArrays>(shader.generated));
    }

    TEST(TypedShaderInputTest, SelectedEntryPointDeterminesItsOwnInterface) {
        const auto path = vsl_test::shader_path("typed_entry_points.spv");
        Reflector first({}, path, "first", vsl::ShaderType::VERTEX);
        Reflector second({}, path, "second", vsl::ShaderType::VERTEX);
        ASSERT_EQ(first.generated.vertex_inputs.size(), 1u);
        ASSERT_EQ(second.generated.vertex_inputs.size(), 1u);
        EXPECT_EQ(first.generated.vertex_inputs[0].location, 0u);
        EXPECT_EQ(second.generated.vertex_inputs[0].location, 7u);
        using AtZero = Input<t::VertexInputDefinition<f::FLOAT_RG, 0>>;
        using AtSeven = Input<t::VertexInputDefinition<f::FLOAT_RG, 7>>;
        EXPECT_NO_THROW(t::validate_vertex_shader<AtZero>(first.generated));
        EXPECT_NO_THROW(t::validate_vertex_shader<AtSeven>(second.generated));
        EXPECT_THROW(t::validate_vertex_shader<AtZero>(second.generated), std::invalid_argument);
        EXPECT_THROW(Reflector({}, path, "absent", vsl::ShaderType::VERTEX), vsl::exceptions::RuntimeException);
        EXPECT_THROW(Reflector({}, path, "first", vsl::ShaderType::FRAGMENT), vsl::exceptions::RuntimeException);
    }

    TEST(TypedShaderInputTest, NonVertexEntryPointsAreRejectedAndProceduralInputIsAccepted) {
        Reflector fragment({}, vsl_test::shader_path("triangle.frag.spv"));
        Reflector procedural({}, vsl_test::shader_path("triangle.vert.spv"));
        EXPECT_THROW(t::validate_vertex_shader<Ordinary>(fragment.generated), std::invalid_argument);
        EXPECT_NO_THROW(t::validate_vertex_shader<t::TypedVertexInput<>>(procedural.generated));
    }
    TEST(TypedShaderInputTest, VertexOnlyScopeDoesNotRequireDescriptorRuntimeArrayCounts) {
        Reflector shader({}, vsl_test::shader_path("typed_runtime_descriptors.vert.spv"),
            "main", vsl::ShaderType::VERTEX, Reflector::Scope::VERTEX_INPUT);
        using Position = Input<t::VertexInputDefinition<f::FLOAT_RG, 0>>;
        EXPECT_NO_THROW(t::validate_vertex_shader<Position>(shader.generated));
        EXPECT_TRUE(shader.generated.binding_points.empty());
    }

    TEST(TypedShaderInputTest, DefaultEntryPointNameDescribesTheReflectedInterface) {
        Reflector shader({}, vsl_test::shader_path("typed_entry_points.spv"));
        ASSERT_EQ(shader.generated.vertex_inputs.size(), 1u);
        if (shader.generated.entry_point == "first")
            EXPECT_EQ(shader.generated.vertex_inputs[0].location, 0u);
        else {
            EXPECT_EQ(shader.generated.entry_point, "second");
            EXPECT_EQ(shader.generated.vertex_inputs[0].location, 7u);
        }
    }

    TEST(TypedShaderInputTest, ModulesWithoutEntryPointsAreRejected) {
        const auto bytes = vsl_test::read_binary(vsl_test::shader_path("typed_entry_points.spv"));
        std::vector<std::uint32_t> words(bytes.size() / sizeof(std::uint32_t));
        std::memcpy(words.data(), bytes.data(), bytes.size());
        std::vector<std::uint32_t> no_entries(words.begin(), words.begin() + 5);
        for (size_t i = 5; i < words.size();) {
            const auto count = words[i] >> 16;
            ASSERT_GT(count, 0u);
            if ((words[i] & 0xffffu) != 15u) // OpEntryPoint
                no_entries.insert(no_entries.end(), words.begin() + i, words.begin() + i + count);
            i += count;
        }
        EXPECT_THROW(Reflector({}, std::span<const std::uint32_t>(no_entries)), vsl::exceptions::RuntimeException);
    }

}
