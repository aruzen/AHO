#pragma once
#include "../operation/render_pass_description.hpp"
#include "../lifetime/owner_ref.hpp"
#include "../define.hpp"
#include "../format.hpp"
#include "buffer_and_image_accessor.hpp"
#include "pv.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace VSL_NAMESPACE {
    struct SwapchainAccessor;
    struct CommandManager;
    struct FenceHolder;

    struct RenderPassAccessor {
        std::shared_ptr<impl::RenderPassImpl> data_;
        const RenderPassDescription& description() const&;
        const RenderPassDescription& description() const&& = delete;
        LogicalDeviceAccessor get_device() const;
        lifetime::OwnerRef owner_ref() const;
    };
    struct RenderPass : RenderPassAccessor {
        RenderPass(LogicalDeviceAccessor device, RenderPassDescription description);
        // 従来の画面表示用アタッチメント1つを持つ構成との互換性を保つ。
        RenderPass(SwapchainAccessor swapchain);
    };

    // 互換性のためのクラス。新しい用途ではRenderPass、Image、FrameBufferを組み合わせる。
    struct IDPickingRenderPass : RenderPassAccessor {
        std::vector<ImageAccessor> picking_buffer;
        IDPickingRenderPass(SwapchainAccessor swapchain);
        void recreate_buffer(std::uint32_t width, std::uint32_t height);
        std::uint32_t read(CommandManager manager, FenceHolder in_flight, std::uint32_t x, std::uint32_t y);
    };
} // namespace VSL_NAMESPACE
