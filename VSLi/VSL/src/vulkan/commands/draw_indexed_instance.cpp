#include <VSL/vulkan/commands/draw_indexed_instance.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/phase.hpp>
#include "draw_count.hpp"

VSL_NAMESPACE::command::DrawInstance::DrawInstance(size_t index_size, size_t instance_size)
    : index_size_(_impl::helper::checkedDrawCount(index_size)), instance_size_(_impl::helper::checkedDrawCount(instance_size)) {}

VSL_NAMESPACE::command::DrawInstance::DrawInstance(size_t instance_size)
    : DrawInstance(0, instance_size) {}

void VSL_NAMESPACE::command::DrawInstance::setIndexSize(size_t size) {
    index_size_ = _impl::helper::checkedDrawCount(size);
}

void VSL_NAMESPACE::command::DrawInstance::setInstanceSize(size_t size) {
    instance_size_ = _impl::helper::checkedDrawCount(size);
}

void VSL_NAMESPACE::command::DrawInstance::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].drawIndexed(index_size_, instance_size_, 0, 0, 0);
}

/*
VSL_NAMESPACE::command::DrawInstanceManip VSL_NAMESPACE::command::drawInstance;

void vsl::command::DrawIndexedManip::setVertexSize(std::optional<size_t> vertexSize) {
    if (vertexSize)
        size = vertexSize.value();
}
 */
