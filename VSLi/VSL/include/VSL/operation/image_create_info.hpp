#pragma once
#include "../vulkan/buffer_and_image_accessor.hpp"

namespace VSL_NAMESPACE {
    struct ImageCreateInfo {
        ImageType usage;
        std::uint32_t width, height;
        data_format::Format format = data_format::SRGB8_RGBA;
        std::uint32_t layers = 1;
        SampleCount samples = SampleCount::ONE;
        MemoryProperty properties = MemoryProperty::DEVICE_LOCAL;
        ImageLayout initial_layout = ImageLayout::UNDEFINED;
    };
} // namespace VSL_NAMESPACE
