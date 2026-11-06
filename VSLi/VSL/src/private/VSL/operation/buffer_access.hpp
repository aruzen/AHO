#pragma once
#include <VSL/define.hpp>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

namespace VSL_NAMESPACE {
    // 同期側が決定し、Vulkan操作側が適用する値。所有者や資源実体を保持しない。
    struct BufferCopyVisibility {
        vk::PipelineStageFlags before_source, before_destination, after_destination;
        vk::AccessFlags source_access, destination_access, after_access;
    };
} // namespace VSL_NAMESPACE
