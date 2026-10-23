//
// Created by morimoto_hibiki on 2025/10/17.
//

#ifndef VSL_SPIRV_REFLECTOR_UTIL
#define VSL_SPIRV_REFLECTOR_UTIL

#include "../define.hpp"

#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

#include "../vulkan/device.hpp"
#include "../vulkan/descriptor.hpp"
#include "../vulkan/stages/push_contents.hpp"
#include "../vulkan/stages/vertex_input.hpp"

namespace VSL_NAMESPACE::utils {
    class SPIRVReflector {
    public:
        enum class Scope {
            ALL,
            VERTEX_INPUT
        };
        enum class NumericType {
            FLOATING_POINT,
            SIGNED_INTEGER,
            UNSIGNED_INTEGER,
            UNSUPPORTED
        };
        struct VertexInputVariable {
            std::string name;
            std::uint32_t location, component, width, components;
            NumericType numeric_type;
        };
        struct GenerateData {
            LogicalDeviceAccessor logical_device;

            std::map<size_t, std::vector<VSL_NAMESPACE::graphic_resource::BindingPoint>> binding_points;

            std::optional<VSL_NAMESPACE::pipeline_layout::PushConstants> push_constants = std::nullopt;
            std::optional<VSL_NAMESPACE::pipeline_layout::VertexInput> vertex_input = std::nullopt;

            ShaderFlag shader_type = ShaderFlag::ERROR_;
            std::string entry_point;
            // Active inputs of the selected entry point, expanded into array elements/matrix columns.
            std::vector<VertexInputVariable> vertex_inputs;

            // レイアウトの添字はディスクリプターセット番号に対応し、欠番には空のレイアウトを配置する。
            // repeatで指定した回数だけ、セット全体の並びを追加で複製する。
            std::vector<VSL_NAMESPACE::graphic_resource::BindingLayout> make_binding_layout(size_t repeat = 0);
        } generated;

        SPIRVReflector(LogicalDeviceAccessor logical_device, std::filesystem::path path,
                       std::string_view entry_point = {}, ShaderType stage = ShaderType::ERROR_, Scope scope = Scope::ALL);

        SPIRVReflector(LogicalDeviceAccessor logical_device, const std::vector<char>& binary,
                       std::string_view entry_point = {}, ShaderType stage = ShaderType::ERROR_, Scope scope = Scope::ALL);

        SPIRVReflector(LogicalDeviceAccessor logical_device, std::span<const std::uint32_t> words,
                       std::string_view entry_point = {}, ShaderType stage = ShaderType::ERROR_, Scope scope = Scope::ALL);

    private:
        void init(std::vector<std::uint32_t> words, std::string_view entry_point, ShaderType stage, Scope scope);
    };
} // namespace VSL_NAMESPACE::utils

#endif //VSL_SPIRV_REFLECTOR_UTIL
