//
// Created by morimoto_hibiki on 2025/10/17.
//

#ifndef VSL_SPIRV_REFLECTOR_UTIL
#define VSL_SPIRV_REFLECTOR_UTIL

#include "../define.hpp"

#include <filesystem>
#include <vector>

#include "../vulkan/device.hpp"
#include "../vulkan/descriptor.hpp"
#include "../vulkan/stages/push_contents.hpp"
#include "../vulkan/stages/vertex_input.hpp"

namespace VSL_NAMESPACE::utils {
    class SPIRVReflector {
    public:
        struct GenerateData {
            LogicalDeviceAccessor logical_device;

            std::map<size_t, std::vector<vsl::graphic_resource::BindingPoint>> binding_points;
            vsl::pipeline_layout::PushConstants push_constants;
            vsl::pipeline_layout::VertexInput vertex_input;

            std::vector<vsl::graphic_resource::BindingLayout> makeBindingLayout(size_t repeat = 0);
        } generated;

        SPIRVReflector(LogicalDeviceAccessor logical_device, std::filesystem::path path);

        SPIRVReflector(LogicalDeviceAccessor logical_device, const std::vector<char> &binary);
    private:
        void init(const std::uint32_t *bp, size_t size);
    };
}

#endif //VSL_SPIRV_REFLECTOR_UTIL
