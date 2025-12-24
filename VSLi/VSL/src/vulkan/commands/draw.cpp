#include <VSL/vulkan/commands/draw.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/phase.hpp>

VSL_NAMESPACE::command::DrawManip VSL_NAMESPACE::command::draw;

VSL_NAMESPACE::command::Draw::Draw(size_t size) : size(size) {}

void VSL_NAMESPACE::command::Draw::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager){
	buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].draw(size, 1, 0, 0);
}

void vsl::command::DrawManip::invoke(vsl::CommandPool pool, vsl::CommandBuffer buffer, vsl::CommandManager manager) {
    buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].draw(size.value(), 1, 0, 0);
}

void vsl::command::DrawManip::setVertexSize(std::optional<size_t> vertexSize) {
    size = vertexSize;
}
