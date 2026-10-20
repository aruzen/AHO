#include <VSL/vulkan/commands/draw.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/phase.hpp>
#include "draw_count.hpp"

VSL_NAMESPACE::command::DrawManip VSL_NAMESPACE::command::draw;

VSL_NAMESPACE::command::Draw::Draw(size_t size) : size_(_impl::helper::checkedDrawCount(size)) {}

void VSL_NAMESPACE::command::Draw::setSize(size_t size) {
    size_ = _impl::helper::checkedDrawCount(size);
}

void VSL_NAMESPACE::command::Draw::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].draw(size_, 1, 0, 0);
}

void VSL_NAMESPACE::command::DrawManip::invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) {
    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].draw(size_.value(), 1, 0, 0);
}

void VSL_NAMESPACE::command::DrawManip::setVertexSize(std::optional<size_t> vertexSize) {
    if (vertexSize)
        size_ = _impl::helper::checkedDrawCount(*vertexSize);
    else
        size_.reset();
}
