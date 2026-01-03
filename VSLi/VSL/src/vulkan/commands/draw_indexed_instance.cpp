#include <VSL/vulkan/commands/draw_indexed_instance.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/phase.hpp>


VSL_NAMESPACE::command::DrawInstance::DrawInstance(size_t index_size, size_t instance_size)
        : index_size(index_size), instance_size(instance_size) {}

VSL_NAMESPACE::command::DrawInstance::DrawInstance(size_t instance_size)
        : instance_size(instance_size) {}

void VSL_NAMESPACE::command::DrawInstance::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].drawIndexed(index_size, instance_size, 0, 0, 0);
}

/*
VSL_NAMESPACE::command::DrawInstanceManip VSL_NAMESPACE::command::drawInstance;

void vsl::command::DrawIndexedManip::setVertexSize(std::optional<size_t> vertexSize) {
    if (vertexSize)
        size = vertexSize.value();
}
 */
