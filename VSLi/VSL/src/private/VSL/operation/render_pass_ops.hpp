#pragma once
#include <VSL/operation/render_pass_description.hpp>
#include <VSL/vulkan/_pimpls.hpp>

namespace VSL_NAMESPACE::operation {
    // targetは未初期化の実体。Deviceの寿命は呼出側がnative破棄まで保持する。
    void initialize_render_pass(impl::RenderPassImpl& target, impl::LogicalDeviceImpl& device, RenderPassDescription description);
    void destroy_render_pass(impl::RenderPassImpl& target) noexcept;
} // namespace VSL_NAMESPACE::operation
