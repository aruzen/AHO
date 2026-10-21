#include <VSL/vulkan/commands/draw_indexed.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/phase.hpp>
#include "draw_count.hpp"

VSL_NAMESPACE::command::DrawIndexedManip VSL_NAMESPACE::command::drawIndexed;

VSL_NAMESPACE::command::DrawIndexed::DrawIndexed(size_t size) : size_(_impl::helper::checkedDrawCount(size)) {}

void VSL_NAMESPACE::command::DrawIndexed::setSize(size_t size) {
    size_ = _impl::helper::checkedDrawCount(size);
}

void VSL_NAMESPACE::command::DrawIndexed::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].drawIndexed(size_, 1, 0, 0, 0);
}

void VSL_NAMESPACE::command::DrawIndexedManip::setVertexSize(std::optional<size_t> vertexSize) {
    if (vertexSize)
        setSize(*vertexSize);
}
