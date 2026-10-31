#pragma once
#include "buffer_and_image_accessor.hpp"

namespace VSL_NAMESPACE {
    struct BufferCreateInfo {
        size_t bytes;
        MemoryType usage;
        MemoryProperty properties = MemoryProperty::HOST_VISIBLE;
        SharingMode sharing = SharingMode::EXCLUSIVE;
    };
} // namespace VSL_NAMESPACE
