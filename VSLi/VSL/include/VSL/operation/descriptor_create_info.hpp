#pragma once
#include "../vulkan/descriptor.hpp"
#include <map>
#include <vector>

namespace VSL_NAMESPACE {
    struct DescriptorLayoutCreateInfo {
        std::vector<graphic_resource::BindingPoint> bindings;
    };
    struct DescriptorPoolCreateInfo {
        std::map<graphic_resource::Type, size_t> descriptors;
        size_t max_sets = graphic_resource::MAX_POOL_SIZE;
    };
} // namespace VSL_NAMESPACE
