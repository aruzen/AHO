#include <VSL/vulkan/commands/draw_indexed_instance.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/phase.hpp>
#include "draw_count.hpp"

VSL_NAMESPACE::command::DrawInstance::DrawInstance(size_t index_size, size_t instance_size)
    : index_size(index_size), instance_size(instance_size) {}

VSL_NAMESPACE::command::DrawInstance::DrawInstance(size_t instance_size)
    : instance_size(instance_size) {}

void VSL_NAMESPACE::command::DrawInstance::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    const auto index_count = _impl::helper::checkedDrawCount(index_size);
    const auto instance_count = _impl::helper::checkedDrawCount(instance_size);
    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].drawIndexed(index_count, instance_count, 0, 0, 0);
}

/*
VSL_NAMESPACE::command::DrawInstanceManip VSL_NAMESPACE::command::drawInstance;

void vsl::command::DrawIndexedManip::setVertexSize(std::optional<size_t> vertexSize) {
    if (vertexSize)
        size = vertexSize.value();
}
 */
