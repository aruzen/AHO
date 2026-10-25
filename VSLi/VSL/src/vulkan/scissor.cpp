#include <VSL/vulkan/scissor.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
    std::uint32_t checked_scissor_component(float value) {
        // 小数部を切り捨てた結果が非負かつint32の範囲に収まる値だけ変換する。
        if (!std::isfinite(value) || value <= -1 || static_cast<double>(value) > std::numeric_limits<std::int32_t>::max())
            throw std::invalid_argument("Scissor component cannot be represented as a nonnegative int32 value");
        return static_cast<std::uint32_t>(value);
    }

    void validate_scissor(const VSL_NAMESPACE::Scissor& scissor) {
        constexpr auto maximum = std::numeric_limits<std::int32_t>::max();
        if (scissor.x < 0 || scissor.y < 0 || scissor.width > std::uint32_t(maximum - scissor.x) || scissor.height > std::uint32_t(maximum - scissor.y))
            throw std::invalid_argument("Scissor endpoints must fit nonnegative int32 coordinates");
    }
} // namespace

VSL_NAMESPACE::Scissor::Scissor() {}

VSL_NAMESPACE::Scissor::Scissor(float x, float y, float width, float height)
    : x(checked_scissor_component(x)), y(checked_scissor_component(y)), width(checked_scissor_component(width)), height(checked_scissor_component(height)) {
    validate_scissor(*this);
}

VSL_NAMESPACE::Scissor::Scissor(VSLD2RectangleAccessor<float> rect) : Scissor(rect.x, rect.y, rect.width, rect.height) {}

VSL_NAMESPACE::Scissor::Scissor(SwapchainAccessor swapchain) : x(0), y(0), height(swapchain.data_->swap_chain_extent.height), width(swapchain.data_->swap_chain_extent.width) {}

void VSL_NAMESPACE::Scissor::injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl) {
    validate_scissor(*this);
    auto& info = *pl.data_->info;

    vk::Rect2D scissor;
    scissor.offset = vk::Offset2D{x, y};
    scissor.extent = vk::Extent2D{width, height};
    info.scissors.push_back(scissor);
}

void VSL_NAMESPACE::Scissor::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
    validate_scissor(*this);
    vk::Rect2D scissor;
    scissor.offset = vk::Offset2D{x, y};
    scissor.extent = vk::Extent2D{width, height};

    buffer.data_->command_buffers[buffer.get_current_buffer_idx()].setScissor(0, scissor);
    // buffer.data_->command_buffers[buffer.get_current_buffer_idx()].setScissorWithCount({ scissor });
}
