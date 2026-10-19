#include <VSL/vulkan/commands/draw_indexed.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/phase.hpp>
#include "draw_count.hpp"

VSL_NAMESPACE::command::DrawIndexedManip VSL_NAMESPACE::command::drawIndexed;

VSL_NAMESPACE::command::DrawIndexed::DrawIndexed(size_t size) : size(size) {}

void VSL_NAMESPACE::command::DrawIndexed::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    const auto count = _impl::helper::checkedDrawCount(size);
    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].drawIndexed(count, 1, 0, 0, 0);
}

void VSL_NAMESPACE::command::DrawIndexedManip::setVertexSize(std::optional<size_t> vertexSize) {
    if (vertexSize)
        size = vertexSize.value();
}
