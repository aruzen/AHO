#pragma once
#include "../operation/render_pass_description.hpp"
#include "../lifetime/owner_ref.hpp"
#include "../lifetime/resource_identity.hpp"
#include "../define.hpp"
#include "../format.hpp"
#include "buffer_and_image_accessor.hpp"
#include "pv.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace VSL_NAMESPACE::lifetime::detail {
    struct GraphicsAccess;
}

namespace VSL_NAMESPACE {
    struct SwapchainAccessor;
    struct CommandManager;
    struct FenceHolder;

    struct RenderPassAccessor {
    private:
        friend struct lifetime::detail::GraphicsAccess;
        lifetime::OwnedRef<impl::RenderPassImpl> _owner;
        explicit RenderPassAccessor(lifetime::OwnedRef<impl::RenderPassImpl> owner) : _owner(std::move(owner)) {}

    public:
        RenderPassAccessor() = default;
        RenderPassAccessor(const RenderPassAccessor&) = default;
        RenderPassAccessor(RenderPassAccessor&&) noexcept = default;
        RenderPassAccessor& operator=(RenderPassAccessor replacement) noexcept {
            swap(replacement);
            return *this;
        }
        explicit operator bool() const noexcept { return bool(_owner); }
        void swap(RenderPassAccessor& other) noexcept { _owner.swap(other._owner); }
        RenderPassIdentity identity() const;
        const RenderPassDescription& description() const&;
        const RenderPassDescription& description() const&& = delete;
        LogicalDeviceAccessor get_device() const;
        lifetime::OwnerRef owner_ref() const;
    };
    struct RenderPass : RenderPassAccessor {
        RenderPass(const RenderPass&) = default;
        RenderPass(RenderPass&&) noexcept = default;
        RenderPass& operator=(RenderPass replacement) noexcept {
            swap(replacement);
            return *this;
        }
        RenderPass(LogicalDeviceAccessor device, RenderPassDescription description);
        // 従来の画面表示用アタッチメント1つを持つ構成との互換性を保つ。
        RenderPass(SwapchainAccessor swapchain);
    };

    // 互換性のためのクラス。新しい用途ではRenderPass、Image、FrameBufferを組み合わせる。
    struct IDPickingRenderPass {
    private:
        friend struct lifetime::detail::GraphicsAccess;
        RenderPassAccessor _pass;
        std::vector<ImageAccessor> _picking_buffer;

    public:
        IDPickingRenderPass(SwapchainAccessor swapchain);
        IDPickingRenderPass(const IDPickingRenderPass&) = default;
        IDPickingRenderPass(IDPickingRenderPass&& other) noexcept;
        IDPickingRenderPass& operator=(IDPickingRenderPass replacement) noexcept;
        ~IDPickingRenderPass();
        void swap(IDPickingRenderPass& other) noexcept;
        operator RenderPassAccessor() const { return _pass; }
        explicit operator bool() const noexcept { return bool(_pass); }
        RenderPassIdentity identity() const { return _pass.identity(); }
        LogicalDeviceAccessor get_device() const { return _pass.get_device(); }
        lifetime::OwnerRef owner_ref() const { return _pass.owner_ref(); }
        const RenderPassDescription& description() const& { return _pass.description(); }
        const RenderPassDescription& description() const&& = delete;
        size_t image_count() const noexcept { return _picking_buffer.size(); }
        ImageAccessor picking_image(size_t index) const { return _picking_buffer.at(index); }
        std::span<const ImageAccessor> picking_images() const& { return _picking_buffer; }
        std::span<const ImageAccessor> picking_images() const&& = delete;
        void recreate_buffer(std::uint32_t width, std::uint32_t height);
        std::uint32_t read(CommandManager manager, FenceHolder in_flight, std::uint32_t x, std::uint32_t y);
    };
} // namespace VSL_NAMESPACE
