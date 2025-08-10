//
// Created by morimoto_hibiki on 2025/07/31.
//

#ifdef _MSC_VER
#include "pch.h"
#endif

#include "../_pimpls.h"

#include "dispatch.hpp"

vsl::command::Dispatch::Dispatch(std::uint32_t x, std::uint32_t y=1, std::uint32_t z=1)
        : x(x), y(y), z(z) {}

void vsl::command::Dispatch::invoke(vsl::CommandPool pool, vsl::CommandBuffer buffer, vsl::CommandManager manager) {
    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].dispatch(x, y, z);
}
