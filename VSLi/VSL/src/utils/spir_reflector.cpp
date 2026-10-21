//
// Created by morimoto_hibiki on 2025/10/17.
//

#include <VSL/utils/spir_reflector.hpp>
#include <VSL/vulkan/_pimpls.hpp>

#include <spirv_cross/spirv_cross.hpp>
#include <fstream>
#include <utility>
#include <ranges>
#include <limits>
#include <cstring>

namespace {
    void validate_spirv_size(std::size_t size) {
        if (size < 5 * sizeof(std::uint32_t) || size % sizeof(std::uint32_t) != 0)
            throw VSL_NAMESPACE::exceptions::RuntimeException("SPIRVReflector", "Invalid SPIR-V binary size: expected at least five complete 32-bit words");
    }

    std::vector<std::uint32_t> read_spirv_words(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file)
            throw VSL_NAMESPACE::exceptions::RuntimeException("SPIRVReflector", "Failed to open SPIR-V file: " + path.string());
        const std::streamoff size = file.tellg();
        if (!std::in_range<std::size_t>(size) || !std::in_range<std::streamsize>(size))
            throw VSL_NAMESPACE::exceptions::RuntimeException("SPIRVReflector", "Invalid SPIR-V file size: " + path.string());
        validate_spirv_size(static_cast<std::size_t>(size));
        std::vector<std::uint32_t> words(static_cast<std::size_t>(size) / sizeof(std::uint32_t));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char*>(words.data()), static_cast<std::streamsize>(size)))
            throw VSL_NAMESPACE::exceptions::RuntimeException("SPIRVReflector", "Failed to read SPIR-V file: " + path.string());
        return words;
    }

    struct ReflectionCompiler : spirv_cross::Compiler {
        using Compiler::Compiler;
        using Compiler::evaluate_constant_u32;
    };

    std::uint32_t descriptor_count(const ReflectionCompiler& compiler, const spirv_cross::SPIRType& type) {
        std::uint32_t count = 1;
        for (std::size_t i = 0; i < type.array.size(); ++i) {
            // No specialization overrides are supplied to this API; reflect the shader defaults.
            const auto extent = type.array_size_literal[i] ? type.array[i] : compiler.evaluate_constant_u32(type.array[i]);
            if (extent == 0)
                throw VSL_NAMESPACE::exceptions::RuntimeException("SPIRVReflector", "Descriptor runtime arrays require an explicit descriptor count");
            if (extent > std::numeric_limits<std::uint32_t>::max() / count)
                throw VSL_NAMESPACE::exceptions::RuntimeException("SPIRVReflector", "Descriptor array count overflow");
            count *= extent;
        }
        return count;
    }
} // namespace

VSL_NAMESPACE::graphic_resource::Type spir_type_to_graphic_type(const spirv_cross::SPIRType& type) {
    namespace gfx_src = VSL_NAMESPACE::graphic_resource;
    switch (type.basetype) {
        case spirv_cross::SPIRType::Image:
            return gfx_src::Type::SAMPLED_IMAGE;
        case spirv_cross::SPIRType::SampledImage:
            return type.image.dim == spv::DimBuffer ? gfx_src::Type::UNIFORM_TEXEL_BUFFER : gfx_src::Type::COMBINED_IMAGE_SAMPLER;
        case spirv_cross::SPIRType::Sampler:
            return gfx_src::Type::SAMPLER;
        default:
            VSL_NAMESPACE::loggingln("error: Unknown spir-v type. (spir_type_to_graphic_type)");
    }
    return gfx_src::Type::MAX_ENUM;
}

VSL_NAMESPACE::utils::SPIRVReflector::SPIRVReflector(LogicalDeviceAccessor logical_device, std::filesystem::path path)
    : generated{std::move(logical_device)} {
    init(read_spirv_words(path));
}

VSL_NAMESPACE::utils::SPIRVReflector::SPIRVReflector(LogicalDeviceAccessor logical_device, const std::vector<char>& binary)
    : generated{std::move(logical_device)} {
    validate_spirv_size(binary.size());
    std::vector<std::uint32_t> words(binary.size() / sizeof(std::uint32_t));
    std::memcpy(words.data(), binary.data(), binary.size());
    init(std::move(words));
}

void VSL_NAMESPACE::utils::SPIRVReflector::init(std::vector<std::uint32_t> words) {
    using namespace graphic_resource;
    ReflectionCompiler comp(std::move(words));

    ShaderFlag shader_type = ShaderFlag::ERROR_;
    switch (comp.get_execution_model()) {
        case spv::ExecutionModelVertex:
            shader_type = ShaderFlag::VERTEX;
            break;
        case spv::ExecutionModelTessellationControl:
            shader_type = ShaderFlag::TESSELLATION_CONTROL;
            break;
        case spv::ExecutionModelTessellationEvaluation:
            shader_type = ShaderFlag::TESSELLATION_EVALUATION;
            break;
        case spv::ExecutionModelGeometry:
            shader_type = ShaderFlag::GEOMETRY;
            break;
        case spv::ExecutionModelRayGenerationKHR:
            shader_type = ShaderFlag::RAYGEN;
            break;
        case spv::ExecutionModelIntersectionKHR:
            shader_type = ShaderFlag::INTERSECTION;
            break;
        case spv::ExecutionModelAnyHitKHR:
            shader_type = ShaderFlag::ANY_HIT;
            break;
        case spv::ExecutionModelClosestHitKHR:
            shader_type = ShaderFlag::CLOSEST_HIT;
            break;
        case spv::ExecutionModelMissKHR:
            shader_type = ShaderFlag::MISS;
            break;
        case spv::ExecutionModelCallableKHR:
            shader_type = ShaderFlag::CALLABLE;
            break;
        case spv::ExecutionModelTaskNV:
        case spv::ExecutionModelTaskEXT:
            shader_type = ShaderFlag::TASK;
            break;
        case spv::ExecutionModelMeshNV:
        case spv::ExecutionModelMeshEXT:
            shader_type = ShaderFlag::MESH;
            break;
        case spv::ExecutionModelFragment:
            shader_type = ShaderFlag::FRAGMENT;
            break;
        case spv::ExecutionModelGLCompute:
            shader_type = ShaderFlag::COMPUTE;
            break;
        default:
            std::cout << "Other shader stage" << std::endl;
            break;
    }

    spirv_cross::ShaderResources shader_resources = comp.get_shader_resources();
    std::vector<std::tuple<spirv_cross::SmallVector<spirv_cross::Resource>, graphic_resource::Type>> mapped_resources = {
        {shader_resources.uniform_buffers, graphic_resource::Type::UNIFORM_BUFFER},
        {shader_resources.storage_buffers, graphic_resource::Type::STORAGE_BUFFER},
        {shader_resources.storage_images, graphic_resource::Type::STORAGE_IMAGE},
        {shader_resources.subpass_inputs, graphic_resource::Type::INPUT_ATTACHMENT},
        {shader_resources.acceleration_structures, graphic_resource::Type::ACCELERATION_STRUCTURE_NV}, // 例: NV拡張
        {shader_resources.separate_images, graphic_resource::Type::SAMPLED_IMAGE},
        {shader_resources.separate_samplers, graphic_resource::Type::SAMPLER},
        /*
            { resources.uniform_texel_buffers, graphic_resource::type::UniformTexelBuffer },
            { resources.storage_texel_buffers, graphic_resource::type::StorageTexelBuffer },
            { resources., graphic_resource::type::Sampler },
            { resources.combined_image_samplers, graphic_resource::type::CombinedImageSampler },
            { resources.input_attachments, graphic_resource::type::InputAttachment },
            { resources.inline_uniform_blocks, graphic_resource::type::InlineUniformBlock },
            // Qcom/Valve拡張は必要に応じて追加
             */
    };

    for (const auto& resource : shader_resources.sampled_images) {
        uint32_t set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
        uint32_t binding = comp.get_decoration(resource.id, spv::DecorationBinding);
        const auto& type = comp.get_type(resource.type_id);
        graphic_resource::Type gfx_type = spir_type_to_graphic_type(type);
        if (gfx_type == graphic_resource::Type::MAX_ENUM)
            VSL_NAMESPACE::loggingln("hint: name: ", resource.name);
        generated.binding_points[set].emplace_back(BindingPoint{
            .binding = binding,
            .binding_type = gfx_type,
            .shader_type = shader_type, // TODO
            .amount = descriptor_count(comp, type),
        });
    }

    for (auto& [resources, gfx_type] : mapped_resources)
        for (const auto& resource : resources) {
            uint32_t set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
            uint32_t binding = comp.get_decoration(resource.id, spv::DecorationBinding);
            const auto& type = comp.get_type(resource.type_id);
            auto descriptor_type = gfx_type;
            if ((gfx_type == Type::SAMPLED_IMAGE || gfx_type == Type::STORAGE_IMAGE) && type.image.dim == spv::DimBuffer)
                descriptor_type = gfx_type == Type::SAMPLED_IMAGE ? Type::UNIFORM_TEXEL_BUFFER : Type::STORAGE_TEXEL_BUFFER;
            generated.binding_points[set].emplace_back(BindingPoint{
                .binding = binding,
                .binding_type = descriptor_type,
                .shader_type = shader_type, // TODO
                .amount = descriptor_count(comp, type),
            });
        }

    for (const auto& resource : shader_resources.push_constant_buffers) {
        const auto& type = comp.get_type(resource.type_id);
        /*
        std::cout << "PushConstant name: " << resource.name << std::endl;
        std::cout << "id: " << resource.id << ", type_id: " << resource.type_id << std::endl;
        std::cout << "PushConstant has " << type.member_types.size() << " members\n";
         */
        for (size_t i = 0; i < type.member_types.size(); i++) {
            /*
            std::cout << "  member[" << i << "] = " << comp.get_member_name(resource.base_type_id, i)
                      << ", offset = " << comp.type_struct_member_offset(type, i)
                      << ", size = " << comp.get_declared_struct_member_size(type, i)
                      << std::endl;
                      */
            if (not generated.push_constants)
                generated.push_constants.emplace();
            generated.push_constants->definitions.emplace_back(pipeline_layout::PushContentDefinition{
                .target_shader = shader_type, // TODO
                .offset = static_cast<uint32_t>(comp.type_struct_member_offset(type, (uint32_t)i)),
                .size = static_cast<uint32_t>(comp.get_declared_struct_member_size(type, (uint32_t)i)),
            });
        }
    }

    if (shader_type != ShaderFlag::VERTEX)
        return;

    for (auto& resource : shader_resources.stage_inputs) {
        const auto& type = comp.get_type(resource.type_id);
        /*
        std::cout << "vertex input name: " << resource.name
        << " width: " << type.width << " vecsize: " << type.vecsize << " columns: " << type.columns << std::endl;
         */
        data_format::Format format = data_format::UNDEFINED;
        switch (type.basetype) {
            case spirv_cross::SPIRType::Short:
                format = data_format::SIGNED_INT16_R;
                break;
            case spirv_cross::SPIRType::UShort:
                format = data_format::UNSIGNED_INT16_R;
                break;
            case spirv_cross::SPIRType::Int64:
                format = data_format::SIGNED_INT64_R;
                break;
            case spirv_cross::SPIRType::UInt64:
                format = data_format::UNSIGNED_INT64_R;
                break;
            case spirv_cross::SPIRType::Half:
                format = data_format::SIGNED_FLOAT16_R;
                break;
            case spirv_cross::SPIRType::Double:
                format = data_format::SIGNED_FLOAT64_R;
                break;
            case spirv_cross::SPIRType::Int:
                format = data_format::INT;
                break;
            case spirv_cross::SPIRType::UInt:
                format = data_format::UNSIGNED_INT;
                break;
            case spirv_cross::SPIRType::Float:
                format = data_format::FLOAT;
                break;
            default:
                loggingln("Warning: Unknown format");
        }
        format = format.to_vec(type.vecsize);
        if (not generated.vertex_input)
            generated.vertex_input.emplace();
        const auto array_elements = descriptor_count(comp, type);
        constexpr auto max_location = std::numeric_limits<std::uint32_t>::max();
        if (type.columns > max_location / array_elements)
            throw exceptions::RuntimeException("SPIRVReflector", "Vertex input array column count overflow");
        const auto columns = type.columns * array_elements;
        const auto location = comp.get_decoration(resource.id, spv::DecorationLocation);
        const auto locations_per_column = type.width == 64 && type.vecsize >= 3 ? 2u : 1u;
        if (columns - 1 > (max_location - location) / locations_per_column)
            throw exceptions::RuntimeException("SPIRVReflector", "Vertex input location range overflow");
        VSL_NAMESPACE::pipeline_layout::VertexInputShapeDefinition definition{
            .layouts = {columns, VSL_NAMESPACE::pipeline_layout::VertexInputLayoutDefinition{.format = format}}};
        for (std::uint32_t column = 0; column < columns; ++column)
            definition.layouts[column].location = location + column * locations_per_column;
        generated.vertex_input->definitions.push_back(std::move(definition));
    }
}

std::vector<VSL_NAMESPACE::graphic_resource::BindingLayout> VSL_NAMESPACE::utils::SPIRVReflector::GenerateData::make_binding_layout(size_t repeat) {
    std::vector<VSL_NAMESPACE::graphic_resource::BindingLayout> result;
    if (binding_points.empty())
        return result;
    const auto last_set = binding_points.rbegin()->first;
    if (last_set >= result.max_size())
        throw VSL_NAMESPACE::exceptions::RuntimeException("SPIRVReflector", "Descriptor set index exceeds vector capacity");
    const auto slots = last_set + 1;
    if (repeat >= result.max_size() / slots)
        throw VSL_NAMESPACE::exceptions::RuntimeException("SPIRVReflector", "Binding layout repeat count exceeds vector capacity");
    if (!logical_device.data_ || !logical_device.data_->device || !logical_device.data_->parent_device)
        throw VSL_NAMESPACE::exceptions::RuntimeException("SPIRVReflector", "A valid logical device is required for binding layouts");
    const auto& physical = logical_device.data_->parent_device;
    const auto max_sets = physical->props ? physical->props->limits.maxBoundDescriptorSets
                                          : physical->device.getProperties().limits.maxBoundDescriptorSets;
    if (slots > max_sets)
        throw VSL_NAMESPACE::exceptions::RuntimeException("SPIRVReflector", "Descriptor set index exceeds device limit");
    result.reserve((repeat + 1) * slots);
    const std::vector<graphic_resource::BindingPoint> empty;
    for (size_t i = 0; i < repeat + 1; i++)
        for (size_t set = 0; set < slots; ++set) {
            const auto found = binding_points.find(set);
            result.emplace_back(logical_device, found == binding_points.end() ? empty : found->second);
        }
    return result;
}
