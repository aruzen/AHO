//
// Created by morimoto_hibiki on 2025/10/17.
//

#include <VSL/utils/spir_reflector.hpp>
#include <VSL/vulkan/_pimpls.hpp>

#include <spirv_cross.hpp>
#include <fstream>
#include <utility>
#include <ranges>
#include <limits>
#include <cstring>
#include <algorithm>

namespace {
    using ReflectedBlock = VSL_NAMESPACE::utils::SPIRVReflector;
    struct ReflectionCompiler : spirv_cross::Compiler {
        using Compiler::Compiler;
        using Compiler::evaluate_constant_u32;
        using Compiler::get_entry_point;
    };

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

    ReflectedBlock::BlockType reflect_block_type(const ReflectionCompiler& compiler, spirv_cross::TypeID id,
                                                 size_t matrix_stride = 0, bool row_major = false) {
        const auto& type = compiler.get_type(id);
        ReflectedBlock::BlockType result;
        if (type.pointer) {
            if (type.storage != spv::StorageClassPhysicalStorageBuffer)
                throw VSL_NAMESPACE::exceptions::RuntimeException("SPIRVReflector", "Unsupported pointer in a shader block");
            result.numeric = ReflectedBlock::NumericType::UNSIGNED_INTEGER;
            result.width = 64;
            result.size = 8;
            return result;
        }
        result.width = type.width;
        result.components = type.vecsize;
        result.columns = type.columns;
        if (!type.array.empty()) {
            result.kind = ReflectedBlock::BlockKind::ARRAY;
            const auto index = type.array.size() - 1;
            result.count = type.array_size_literal[index] ? type.array[index] : compiler.evaluate_constant_u32(type.array[index]);
            result.stride = compiler.get_decoration(id, spv::DecorationArrayStride);
            if (!type.parent_type || !result.stride)
                throw VSL_NAMESPACE::exceptions::RuntimeException("SPIRVReflector", "Block array is missing its element type/ArrayStride");
            result.elements.push_back(reflect_block_type(compiler, type.parent_type, matrix_stride, row_major));
            if (result.count) {
                const auto size = result.elements.front().size;
                if ((result.count - 1) > (std::numeric_limits<size_t>::max() - size) / result.stride)
                    throw VSL_NAMESPACE::exceptions::RuntimeException("SPIRVReflector", "Block array extent overflow");
                result.size = (result.count - 1) * result.stride + size;
            }
            return result;
        }
        if (type.basetype == spirv_cross::SPIRType::Struct) {
            result.kind = ReflectedBlock::BlockKind::STRUCTURE;
            for (std::uint32_t i = 0; i < type.member_types.size(); ++i) {
                const auto stride = compiler.get_member_decoration(type.self, i, spv::DecorationMatrixStride);
                const auto major = compiler.has_member_decoration(type.self, i, spv::DecorationRowMajor);
                auto member = reflect_block_type(compiler, type.member_types[i], stride, major);
                const auto offset = compiler.type_struct_member_offset(type, i);
                if (member.size > std::numeric_limits<size_t>::max() - offset)
                    throw VSL_NAMESPACE::exceptions::RuntimeException("SPIRVReflector", "Block member extent overflow");
                result.size = std::max(result.size, offset + member.size);
                result.members.push_back({compiler.get_member_name(type.self, i), offset,
                                          compiler.get_declared_struct_member_size(type, i), std::move(member)});
            }
            return result;
        }
        using Numeric = ReflectedBlock::NumericType;
        switch (type.basetype) {
            case spirv_cross::SPIRType::Half:
            case spirv_cross::SPIRType::Float:
            case spirv_cross::SPIRType::Double:
                result.numeric = Numeric::FLOATING_POINT;
                break;
            case spirv_cross::SPIRType::SByte:
            case spirv_cross::SPIRType::Short:
            case spirv_cross::SPIRType::Int:
            case spirv_cross::SPIRType::Int64:
                result.numeric = Numeric::SIGNED_INTEGER;
                break;
            case spirv_cross::SPIRType::UByte:
            case spirv_cross::SPIRType::UShort:
            case spirv_cross::SPIRType::UInt:
            case spirv_cross::SPIRType::UInt64:
                result.numeric = Numeric::UNSIGNED_INTEGER;
                break;
            case spirv_cross::SPIRType::Boolean:
                result.numeric = Numeric::UNSIGNED_INTEGER;
                result.width = 32;
                break;
            default:
                break;
        }
        if (result.numeric == Numeric::UNSUPPORTED)
            throw VSL_NAMESPACE::exceptions::RuntimeException("SPIRVReflector", "Unsupported shader block numeric type");
        if (type.columns > 1) {
            if (!matrix_stride)
                throw VSL_NAMESPACE::exceptions::RuntimeException("SPIRVReflector", "Block matrix is missing MatrixStride");
            result.kind = ReflectedBlock::BlockKind::MATRIX;
            result.stride = matrix_stride;
            result.row_major = row_major;
            result.size = ((row_major ? type.vecsize : type.columns) - 1) * matrix_stride + (row_major ? type.columns : type.vecsize) * result.width / 8;
        } else {
            result.kind = type.vecsize > 1 ? ReflectedBlock::BlockKind::VECTOR : ReflectedBlock::BlockKind::SCALAR;
            result.size = type.vecsize * result.width / 8;
        }
        return result;
    }

    ReflectedBlock::Block reflect_block(const ReflectionCompiler& compiler, const spirv_cross::Resource& resource,
                                        VSL_NAMESPACE::ShaderFlag stages, std::optional<VSL_NAMESPACE::graphic_resource::Type> kind) {
        ReflectedBlock::Block result;
        result.name = resource.name;
        result.kind = kind;
        result.stages = stages;
        if (kind) {
            result.set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
            result.binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
        }
        result.declared_size = compiler.get_declared_struct_size(compiler.get_type(resource.base_type_id));
        result.type = reflect_block_type(compiler, resource.base_type_id);
        return result;
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

VSL_NAMESPACE::utils::SPIRVReflector::SPIRVReflector(LogicalDeviceAccessor logical_device, std::filesystem::path path, std::string_view entry_point, ShaderType stage, Scope scope)
    : generated{std::move(logical_device)} {
    init(read_spirv_words(path), entry_point, stage, scope);
}

VSL_NAMESPACE::utils::SPIRVReflector::SPIRVReflector(LogicalDeviceAccessor logical_device, const std::vector<char>& binary, std::string_view entry_point, ShaderType stage, Scope scope)
    : generated{std::move(logical_device)} {
    validate_spirv_size(binary.size());
    std::vector<std::uint32_t> words(binary.size() / sizeof(std::uint32_t));
    std::memcpy(words.data(), binary.data(), binary.size());
    init(std::move(words), entry_point, stage, scope);
}

VSL_NAMESPACE::utils::SPIRVReflector::SPIRVReflector(LogicalDeviceAccessor logical_device, std::span<const std::uint32_t> words,
                                                     std::string_view entry_point, ShaderType stage, Scope scope) : generated{std::move(logical_device)} {
    validate_spirv_size(words.size_bytes());
    init({words.begin(), words.end()}, entry_point, stage, scope);
}

namespace {
    VSL_NAMESPACE::ShaderFlag shader_flag(spv::ExecutionModel model) {
        switch (model) {
            case spv::ExecutionModelVertex:
                return VSL_NAMESPACE::ShaderFlag::VERTEX;
            case spv::ExecutionModelTessellationControl:
                return VSL_NAMESPACE::ShaderFlag::TESSELLATION_CONTROL;
            case spv::ExecutionModelTessellationEvaluation:
                return VSL_NAMESPACE::ShaderFlag::TESSELLATION_EVALUATION;
            case spv::ExecutionModelGeometry:
                return VSL_NAMESPACE::ShaderFlag::GEOMETRY;
            case spv::ExecutionModelRayGenerationKHR:
                return VSL_NAMESPACE::ShaderFlag::RAYGEN;
            case spv::ExecutionModelIntersectionKHR:
                return VSL_NAMESPACE::ShaderFlag::INTERSECTION;
            case spv::ExecutionModelAnyHitKHR:
                return VSL_NAMESPACE::ShaderFlag::ANY_HIT;
            case spv::ExecutionModelClosestHitKHR:
                return VSL_NAMESPACE::ShaderFlag::CLOSEST_HIT;
            case spv::ExecutionModelMissKHR:
                return VSL_NAMESPACE::ShaderFlag::MISS;
            case spv::ExecutionModelCallableKHR:
                return VSL_NAMESPACE::ShaderFlag::CALLABLE;
            case spv::ExecutionModelTaskNV:
            case spv::ExecutionModelTaskEXT:
                return VSL_NAMESPACE::ShaderFlag::TASK;
            case spv::ExecutionModelMeshNV:
            case spv::ExecutionModelMeshEXT:
                return VSL_NAMESPACE::ShaderFlag::MESH;
            case spv::ExecutionModelFragment:
                return VSL_NAMESPACE::ShaderFlag::FRAGMENT;
            case spv::ExecutionModelGLCompute:
                return VSL_NAMESPACE::ShaderFlag::COMPUTE;
            default:
                return VSL_NAMESPACE::ShaderFlag::ERROR_;
        }
    }
} // namespace

void VSL_NAMESPACE::utils::SPIRVReflector::init(std::vector<std::uint32_t> words, std::string_view entry_point, ShaderType stage, Scope scope) try {
    using namespace graphic_resource;
    ReflectionCompiler comp(std::move(words));

    const auto entry_points = comp.get_entry_points_and_stages();
    if (entry_points.empty())
        throw exceptions::RuntimeException("SPIRVReflector", "SPIR-V contains no shader entry point");
    if (!entry_point.empty() || stage != ShaderType::ERROR_) {
        std::optional<spirv_cross::EntryPoint> selected;
        for (const auto& candidate : entry_points) {
            if ((!entry_point.empty() && candidate.name != entry_point) ||
                (stage != ShaderType::ERROR_ && shader_flag(candidate.execution_model) != convert(stage)))
                continue;
            if (selected)
                throw exceptions::RuntimeException("SPIRVReflector", "Ambiguous shader entry point: " + std::string(entry_point));
            selected = candidate;
        }
        if (!selected)
            throw exceptions::RuntimeException("SPIRVReflector", "Shader entry point/stage not found: " + std::string(entry_point));
        comp.set_entry_point(selected->name, selected->execution_model);
    }
    generated.entry_point = comp.get_entry_point().name;
    const auto shader_type = shader_flag(comp.get_execution_model());
    generated.shader_type = shader_type;

    spirv_cross::ShaderResources shader_resources = comp.get_shader_resources();
    if (scope == Scope::ALL) {
        for (const auto& resource : shader_resources.uniform_buffers)
            generated.blocks.push_back(reflect_block(comp, resource, shader_type, Type::UNIFORM_BUFFER));
        for (const auto& resource : shader_resources.storage_buffers)
            generated.blocks.push_back(reflect_block(comp, resource, shader_type, Type::STORAGE_BUFFER));
        for (const auto& resource : shader_resources.push_constant_buffers)
            generated.blocks.push_back(reflect_block(comp, resource, shader_type, std::nullopt));
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
    }

    if (shader_type != ShaderFlag::VERTEX)
        return;

    const auto active_variables = comp.get_active_interface_variables();
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
        for (std::uint32_t column = 0; column < columns; ++column) {
            const auto column_location = location + column * locations_per_column;
            definition.layouts[column].location = column_location;
            if (active_variables.contains(resource.id)) {
                auto numeric = NumericType::UNSUPPORTED;
                switch (type.basetype) {
                    case spirv_cross::SPIRType::Half:
                    case spirv_cross::SPIRType::Float:
                    case spirv_cross::SPIRType::Double:
                        numeric = NumericType::FLOATING_POINT;
                        break;
                    case spirv_cross::SPIRType::Short:
                    case spirv_cross::SPIRType::Int:
                    case spirv_cross::SPIRType::Int64:
                        numeric = NumericType::SIGNED_INTEGER;
                        break;
                    case spirv_cross::SPIRType::UShort:
                    case spirv_cross::SPIRType::UInt:
                    case spirv_cross::SPIRType::UInt64:
                        numeric = NumericType::UNSIGNED_INTEGER;
                        break;
                    default:
                        break;
                }
                generated.vertex_inputs.push_back({resource.name, column_location,
                                                   comp.get_decoration(resource.id, spv::DecorationComponent), type.width, type.vecsize, numeric});
            }
        }
        generated.vertex_input->definitions.push_back(std::move(definition));
    }
}

catch (const spirv_cross::CompilerError& error) {
    throw exceptions::RuntimeException("SPIRVReflector", error.what());
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
