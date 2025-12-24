//
// Created by morimoto_hibiki on 2025/10/17.
//

#include <VSL/utils/spir_reflector.hpp>

#include <spirv_cross/spirv_cross.hpp>
#include <fstream>
#include <utility>

vsl::graphic_resource::Type spir_type_to_graphic_type(const spirv_cross::SPIRType &type) {
    namespace gfx_src = vsl::graphic_resource;
    switch (type.basetype) {
        case spirv_cross::SPIRType::Image:
            return gfx_src::Type::SampledImage;
        case spirv_cross::SPIRType::SampledImage:
            return gfx_src::Type::CombinedImageSampler;
        case spirv_cross::SPIRType::Sampler:
            return gfx_src::Type::Sampler;
        default:
            vsl::loggingln("error: Unknown spir-v type. (spir_type_to_graphic_type)");
    }
    return gfx_src::Type::MaxEnum;
}

vsl::utils::SPIRVReflector::SPIRVReflector(LogicalDeviceAccessor logical_device, std::filesystem::path path)
        : generated{ std::move(logical_device) } {
    std::vector<char> binary;
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw vsl::exceptions::RuntimeException("file", "failed to open file!", std::source_location::current());
        }

        size_t fileSize = (size_t) file.tellg();
        binary.resize(fileSize);
        file.seekg(0);
        file.read(binary.data(), fileSize);
        file.close();
    }
    if (binary.size() % 4 != 0)
        loggingln("warning: SPIRVReflector: SPIR-V file size (not multiple of 4 bytes) by ", path);
    init((const std::uint32_t *) binary.data(), binary.size() / sizeof(std::uint32_t));
}


vsl::utils::SPIRVReflector::SPIRVReflector(LogicalDeviceAccessor logical_device, const std::vector<char> &binary)
        : generated{ std::move(logical_device) } {
    if (binary.size() % 4 != 0)
        loggingln("warning: SPIRVReflector: SPIR-V file size (not multiple of 4 bytes)");
    init((const std::uint32_t *) binary.data(), binary.size() / sizeof(std::uint32_t));
}

void vsl::utils::SPIRVReflector::init(const std::uint32_t *bp, size_t size) {
    using namespace graphic_resource;
    spirv_cross::Compiler comp(bp, size);

    ShaderFlag shaderType = ShaderFlag::Error;
    switch (comp.get_execution_model()) {
        case spv::ExecutionModelVertex:
            shaderType = ShaderFlag::Vertex;
            break;
        case spv::ExecutionModelFragment:
            shaderType = ShaderFlag::Fragment;
            break;
        case spv::ExecutionModelGLCompute:
            shaderType = ShaderFlag::Compute;
            break;
        default:
            std::cout << "Other shader stage" << std::endl;
            break;
    }

    spirv_cross::ShaderResources shader_resources = comp.get_shader_resources();
    std::vector<std::tuple<spirv_cross::SmallVector<spirv_cross::Resource>, graphic_resource::Type>> mapped_resources = {
            {shader_resources.uniform_buffers,         graphic_resource::Type::UniformBuffer},
            {shader_resources.storage_buffers,         graphic_resource::Type::StorageBuffer},
            {shader_resources.storage_images,          graphic_resource::Type::StorageImage},
            {shader_resources.acceleration_structures, graphic_resource::Type::AccelerationStructureNv}, // 例: NV拡張
            {shader_resources.separate_samplers,       graphic_resource::Type::Sampler},
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

    for (const auto &resource: shader_resources.sampled_images) {
        uint32_t set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
        uint32_t binding = comp.get_decoration(resource.id, spv::DecorationBinding);
        const auto &type = comp.get_type(resource.type_id);
        graphic_resource::Type gfx_type = spir_type_to_graphic_type(type);
        if (gfx_type == graphic_resource::Type::MaxEnum)
            vsl::loggingln("hint: name: ", resource.name);
        generated.binding_points[set].emplace_back(BindingPoint{
                .binding = binding,
                .bindingType = gfx_type,
                .shaderType = shaderType, // TODO
                .amount = type.array.empty() ? 1 : (std::uint32_t)type.array.size(),
        });
    }

    for (auto &[resources, gfx_type]: mapped_resources)
        for (const auto &resource: resources) {
            uint32_t set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
            uint32_t binding = comp.get_decoration(resource.id, spv::DecorationBinding);
            const auto &type = comp.get_type(resource.type_id);
            generated.binding_points[set].emplace_back(BindingPoint{
                    .binding = binding,
                    .bindingType = gfx_type,
                    .shaderType = shaderType,  // TODO
                    .amount = type.array.empty() ? 1 : (std::uint32_t)type.array.size(),
            });
        }

    for (const auto &resource: shader_resources.push_constant_buffers) {
        const auto &type = comp.get_type(resource.type_id);
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
                    .targetShader = shaderType, // TODO
                    .offset = static_cast<uint32_t>(comp.type_struct_member_offset(type, i)),
                    .size = static_cast<uint32_t>(comp.get_declared_struct_member_size(type, i)),
            });
        }
    }

    for (auto &resource: shader_resources.stage_inputs) {
        const auto &type = comp.get_type(resource.type_id);
        /*
        std::cout << "vertex input name: " << resource.name
        << " width: " << type.width << " vecsize: " << type.vecsize << " columns: " << type.columns << std::endl;
         */
        data_format::___Format format = data_format::Undefined;
        switch (type.basetype) {
            case spirv_cross::SPIRType::Int:
                format = data_format::Int;
                break;
            case spirv_cross::SPIRType::UInt:
                format = data_format::UnsignedInt;
                break;
            case spirv_cross::SPIRType::Float:
                format = data_format::Float;
                break;
            default:
                loggingln("Warning: Unknown format");
        }
        format = format.toVec(type.vecsize);
        if (not generated.vertex_input)
            generated.vertex_input.emplace();
        for (size_t i = 0; i < type.columns; i++)
            generated.vertex_input->add(format);
    }
}

std::vector<vsl::graphic_resource::BindingLayout> vsl::utils::SPIRVReflector::GenerateData::makeBindingLayout(size_t repeat) {
    std::vector<vsl::graphic_resource::BindingLayout> result;
    for (size_t i = 0; i < repeat + 1; i++)
        for (auto &[_, points]: binding_points)
            result.emplace_back(logical_device, points);
    return result;
}