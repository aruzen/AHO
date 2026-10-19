//
// Created by morimoto_hibiki on 2025/07/31.
//
#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/vulkan/commands/dispatch.hpp>

VSL_NAMESPACE::command::Dispatch::Dispatch(std::uint32_t x, std::uint32_t y, std::uint32_t z)
    : x(x), y(y), z(z) {}

void VSL_NAMESPACE::command::Dispatch::invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].dispatch(x, y, z);
}
