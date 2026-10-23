#include <VSL/utils/typed_shader.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <stdexcept>

namespace {
    using Reflector = VSL_NAMESPACE::utils::SPIRVReflector;
    using Numeric = Reflector::NumericType;

    Numeric numeric_type(VSL_NAMESPACE::data_format::Format format) {
        using enum VSL_NAMESPACE::data_format::FormatTypes;
        switch (format.to_vec<1>().type()) {
            case SIGNED_INT8_R:
            case SIGNED_INT16_R:
            case SIGNED_INT32_R:
            case SIGNED_INT64_R:
                return Numeric::SIGNED_INTEGER;
            case UNSIGNED_INT8_R:
            case UNSIGNED_INT16_R:
            case UNSIGNED_INT32_R:
            case UNSIGNED_INT64_R:
                return Numeric::UNSIGNED_INTEGER;
            case SIGNED_FLOAT16_R:
            case SIGNED_FLOAT32_R:
            case SIGNED_FLOAT64_R:
            case UNSIGNED_NORMALIZED8_R:
            case SIGNED_NORMALIZED8_R:
            case UNSIGNED_SCALED8_R:
            case SIGNED_SCALED8_R:
            case UNSIGNED_NORMALIZED16_R:
            case SIGNED_NORMALIZED16_R:
            case UNSIGNED_SCALED16_R:
            case SIGNED_SCALED16_R:
            case SRGB8_R:
                return Numeric::FLOATING_POINT;
            default:
                return Numeric::UNSUPPORTED;
        }
    }
} // namespace

void VSL_NAMESPACE::typed::detail::validate_vertex_shader(const Reflector::GenerateData& shader,
                                                          std::span<const VertexAttributeLayout> attributes) {
    if (shader.shader_type != ShaderFlag::VERTEX)
        throw std::invalid_argument("Typed vertex input requires a vertex shader entry point");
    for (const auto& input : shader.vertex_inputs) {
        const auto fail = [&](const char* reason) {
            throw std::invalid_argument("Typed vertex input: entry '" + shader.entry_point + "', input '" + input.name +
                                        "', location " + std::to_string(input.location) + ", component " + std::to_string(input.component) + ": " + reason);
        };
        if (input.numeric_type == Numeric::UNSUPPORTED || !input.components || input.components > 4 ||
            (input.width != 16 && input.width != 32 && input.width != 64))
            fail("unsupported shader input type");
        const auto words = input.width == 64 ? 2u : 1u;
        if (input.component >= 4 || (words == 2 && input.component % 2) ||
            (words == 1 && input.component + input.components > 4))
            fail("unsupported shader Component decoration");
        for (std::uint32_t component = 0; component < input.components; ++component) {
            const auto word = input.component + words * component;
            const auto location = std::uint64_t(input.location) + word / 4;
            const VertexAttributeLayout* attribute = nullptr;
            for (const auto& candidate : attributes) {
                if (location >= candidate.location && location < std::uint64_t(candidate.location) + candidate.locations) {
                    attribute = &candidate;
                    break;
                }
            }
            if (!attribute)
                fail("missing vertex attribute");
            if (numeric_type(attribute->format) != input.numeric_type)
                fail("numeric type mismatch (float/signed/unsigned)");
            const bool wide = attribute->format.to_vec<1>().size() == 8;
            if (wide != (input.width == 64))
                fail("64-bit width mismatch");
            // Non-64-bit attributes permit missing components (Vulkan supplies defaults).
            // 64-bit attributes have no defaults, including the second location of dvec3/dvec4.
            if (wide && (location - attribute->location) * 2 + (word % 4) / 2 >= attribute->format.get_vec_length())
                fail("64-bit attribute has insufficient components");
        }
    }
}

void VSL_NAMESPACE::typed::detail::validate_pipeline_vertex_shader(PipelineLayoutAccessor layout,
                                                                   std::span<const VertexAttributeLayout> attributes) {
    if (!layout.data_ || !layout.data_->info)
        throw std::invalid_argument("Typed pipeline requires an initialized layout");
    const impl::ShaderImpl* vertex = nullptr;
    for (const auto& shader : layout.data_->info->shaders) {
        if (shader->shader_stage_create_info.stage != vk::ShaderStageFlagBits::eVertex)
            continue;
        if (vertex)
            throw std::invalid_argument("Typed pipeline requires exactly one vertex shader");
        vertex = shader.get();
    }
    if (!vertex)
        throw std::invalid_argument("Typed pipeline requires exactly one vertex shader");
    Reflector reflection({}, std::span<const std::uint32_t>(vertex->code), vertex->name, ShaderType::VERTEX, Reflector::Scope::VERTEX_INPUT);
    validate_vertex_shader(reflection.generated, attributes);
}
