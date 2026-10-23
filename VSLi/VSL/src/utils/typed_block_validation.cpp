#include <VSL/utils/typed_block_validation.hpp>
#include <VSL/utils/typed_resources.hpp>
#include <VSL/utils/typed_push_constants.hpp>
#include <algorithm>
#include <stdexcept>

namespace {
    using Reflector = VSL_NAMESPACE::utils::SPIRVReflector;
    using CppType = VSL_NAMESPACE::typed::BlockType;
    using CppKind = VSL_NAMESPACE::typed::BlockKind;
    using Numeric = VSL_NAMESPACE::typed::BlockNumeric;
    using ShaderKind = Reflector::BlockKind;
    using ShaderType = Reflector::BlockType;
    std::optional<std::string> compare_layout(const CppType& cpp, const ShaderType& shader, const std::string& path);

    Numeric memory_numeric(Reflector::NumericType value) {
        switch (value) {
            case Reflector::NumericType::FLOATING_POINT:
                return Numeric::FLOATING_POINT;
            case Reflector::NumericType::SIGNED_INTEGER:
                return Numeric::SIGNED_INTEGER;
            case Reflector::NumericType::UNSIGNED_INTEGER:
                return Numeric::UNSIGNED_INTEGER;
            default:
                return Numeric::UNKNOWN;
        }
    }
    std::optional<std::string> compare_layout(const CppType& cpp, const ShaderType& shader, const std::string& path) {
        const auto fail = [&](const std::string& reason) { return std::optional(path + ": " + reason); };
        if (shader.kind == ShaderKind::STRUCTURE) {
            if (cpp.kind != CppKind::STRUCTURE) {
                if (shader.members.size() == 1 && shader.members.front().offset == 0)
                    return compare_layout(cpp, shader.members.front().type, path + "." + shader.members.front().name);
                return fail("C++ structure layout is missing");
            }
            for (const auto& member : shader.members) {
                const auto found = std::ranges::find(cpp.fields, member.offset, &VSL_NAMESPACE::typed::BlockField::offset);
                if (found == cpp.fields.end())
                    return fail("member '" + member.name + "' offset " + std::to_string(member.offset) + " is not present in C++ layout");
                if (auto error = compare_layout(*found->type, member.type, path + "." + member.name))
                    return error;
            }
            return std::nullopt;
        }
        if (shader.kind == ShaderKind::ARRAY) {
            if (cpp.kind != CppKind::ARRAY || !cpp.element || shader.elements.size() != 1)
                return fail("C++ array layout is missing");
            if (cpp.stride != shader.stride)
                return fail("array stride mismatch: shader " + std::to_string(shader.stride) + ", C++ " + std::to_string(cpp.stride));
            if (!cpp.count || (shader.count && cpp.count < shader.count))
                return fail("C++ array capacity is smaller than the shader array");
            return compare_layout(*cpp.element, shader.elements.front(), path + "[]");
        }
        if (shader.kind == ShaderKind::MATRIX) {
            if (cpp.kind != CppKind::MATRIX)
                return fail("C++ matrix requires an explicit BlockMatrix layout");
            if (cpp.stride != shader.stride)
                return fail("matrix stride mismatch: shader " + std::to_string(shader.stride) + ", C++ " + std::to_string(cpp.stride));
            if (cpp.row_major != shader.row_major)
                return fail("matrix row/column major mismatch");
            if (cpp.components != shader.components || cpp.columns != shader.columns)
                return fail("matrix dimensions mismatch");
        } else if (shader.kind == ShaderKind::VECTOR) {
            if (cpp.kind != CppKind::VECTOR || cpp.components < shader.components)
                return fail("C++ vector has insufficient components");
        } else if (cpp.kind != CppKind::SCALAR)
            return fail("C++ scalar layout is missing");
        if (cpp.numeric != memory_numeric(shader.numeric) || cpp.width != shader.width)
            return fail("numeric type/width mismatch");
        return std::nullopt;
    }
    [[noreturn]] void layout_error(const std::string& context, const std::string& reason) {
        throw std::invalid_argument("Typed memory layout: " + context + ": " + reason);
    }
    std::optional<std::string> compare_push_member(const CppType& cpp, size_t base, const Reflector::BlockMember& member, const std::string& path) {
        if (base == member.offset && cpp.kind != CppKind::STRUCTURE)
            return compare_layout(cpp, member.type, path);
        if (cpp.kind == CppKind::STRUCTURE) {
            if (base == member.offset && member.type.kind == ShaderKind::STRUCTURE)
                if (auto error = compare_layout(cpp, member.type, path); !error)
                    return std::nullopt;
            for (const auto& field : cpp.fields)
                if (base <= member.offset && field.offset == member.offset - base)
                    return compare_layout(*field.type, member.type, path);
        }
        return path + ": no C++ member at shader offset " + std::to_string(member.offset);
    }
} // namespace

namespace VSL_NAMESPACE::typed::detail {
    void validate_cpp_block(const Reflector::Block& shader, const BlockType& cpp, size_t payload_size, const std::string& context) {
        if (auto error = compare_layout(cpp, shader.type, context); !error) {
            if (payload_size < shader.type.size)
                layout_error(context, "C++ payload does not cover the shader block");
            return;
        }
        // A bare storage element describes a block consisting of one runtime array at offset zero.
        if (shader.kind == graphic_resource::Type::STORAGE_BUFFER && shader.type.members.size() == 1) {
            const auto& member = shader.type.members.front();
            if (member.offset == 0 && member.type.kind == ShaderKind::ARRAY && member.type.count == 0 && member.type.elements.size() == 1) {
                if (payload_size != member.type.stride)
                    layout_error(context, "storage element sizeof/array stride mismatch");
                if (auto error = compare_layout(cpp, member.type.elements.front(), context + "." + member.name + "[]"))
                    layout_error(context, *error);
                return;
            }
        }
        layout_error(context, *compare_layout(cpp, shader.type, context));
    }
    void validate_descriptor_blocks(const Reflector::GenerateData& shader, std::span<const DescriptorRequirement> requirements) {
        for (const auto& block : shader.blocks) {
            if (!block.kind)
                continue;
            const auto found = std::ranges::find_if(requirements, [&](const auto& requirement) {
                return requirement.set == block.set && requirement.point.binding == block.binding;
            });
            const auto context = "entry '" + shader.entry_point + "', set " + std::to_string(block.set) + ", binding " + std::to_string(block.binding);
            if (found == requirements.end() || !found->memory_layout)
                layout_error(context, "C++ block layout is missing");
            validate_cpp_block(block, *found->memory_layout, found->payload_size, context);
        }
    }
    void validate_push_blocks(const Reflector::GenerateData& shader, std::span<const PushRequirement> requirements) {
        for (const auto& block : shader.blocks) {
            if (block.kind)
                continue;
            for (const auto& member : block.type.members) {
                const auto path = "entry '" + shader.entry_point + "', push member '" + member.name + "'";
                std::optional<std::string> failure;
                bool matched = false;
                for (const auto& requirement : requirements) {
                    if (!contain(requirement.range.target_shader, block.stages) || !requirement.memory_layout)
                        continue;
                    auto error = compare_push_member(*requirement.memory_layout, requirement.range.offset, member, path);
                    if (!error) {
                        matched = true;
                        break;
                    }
                    failure = std::move(error);
                }
                if (!matched)
                    layout_error(path, failure.value_or("C++ push layout is missing"));
            }
        }
    }
} // namespace VSL_NAMESPACE::typed::detail
