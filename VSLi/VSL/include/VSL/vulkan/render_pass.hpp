#pragma once
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

    using AttachmentId = std::uint32_t;
    inline constexpr AttachmentId UNUSED_ATTACHMENT = UINT32_MAX;
    inline constexpr std::uint32_t EXTERNAL_SUBPASS = UINT32_MAX;

    enum class LoadOp {
        LOAD = 0,
        CLEAR = 1,
        DONT_CARE = 2
    };
    enum class StoreOp {
        STORE = 0,
        DONT_CARE = 1
    };
    enum class PipelineStage : std::uint32_t {
        TOP = 0x1,
        VERTEX_SHADER = 0x8,
        FRAGMENT_SHADER = 0x80,
        EARLY_DEPTH_STENCIL = 0x100,
        LATE_DEPTH_STENCIL = 0x200,
        COLOR_ATTACHMENT_OUTPUT = 0x400,
        COMPUTE_SHADER = 0x800,
        TRANSFER = 0x1000,
        BOTTOM = 0x2000,
        HOST = 0x4000,
        ALL_GRAPHICS = 0x8000,
        ALL_COMMANDS = 0x10000
    };
    enum class AccessMask : std::uint32_t {
        NONE = 0,
        SHADER_READ = 0x20,
        SHADER_WRITE = 0x40,
        COLOR_READ = 0x80,
        COLOR_WRITE = 0x100,
        DEPTH_STENCIL_READ = 0x200,
        DEPTH_STENCIL_WRITE = 0x400,
        TRANSFER_READ = 0x800,
        TRANSFER_WRITE = 0x1000,
        HOST_READ = 0x2000,
        HOST_WRITE = 0x4000,
        MEMORY_READ = 0x8000,
        MEMORY_WRITE = 0x10000
    };
    ___VSL_EXPAND_FLAGS_OPERATOR(PipelineStage);
    ___VSL_EXPAND_FLAGS_OPERATOR(AccessMask);

    struct AttachmentDescription {
        data_format::Format format = data_format::SRGB8_RGBA;
        SampleCount samples = SampleCount::ONE;
        LoadOp load = LoadOp::CLEAR;
        StoreOp store = StoreOp::STORE;
        LoadOp stencil_load = LoadOp::DONT_CARE;
        StoreOp stencil_store = StoreOp::DONT_CARE;
        ImageLayout initial_layout = ImageLayout::UNDEFINED;
        ImageLayout final_layout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    };
    struct AttachmentReference {
        AttachmentId attachment;
        ImageLayout layout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    };
    struct SubpassDescription {
        // 配列内の位置がフラグメント出力のlocation、入力アタッチメントのインデックスに対応する。
        std::vector<AttachmentReference> colors;
        std::vector<AttachmentReference> inputs;
        std::vector<AttachmentReference> resolves;
        std::optional<AttachmentReference> depth_stencil;
        std::vector<AttachmentId> preserve;
    };
    struct SubpassDependency {
        std::uint32_t source = EXTERNAL_SUBPASS;
        std::uint32_t destination = 0;
        PipelineStage source_stage = PipelineStage::ALL_GRAPHICS;
        PipelineStage destination_stage = PipelineStage::ALL_GRAPHICS;
        AccessMask source_access = AccessMask::MEMORY_WRITE;
        AccessMask destination_access = AccessMask::MEMORY_READ | AccessMask::MEMORY_WRITE;
        bool by_region = false;
    };
    struct RenderPassDescription {
        std::vector<AttachmentDescription> attachments;
        std::vector<SubpassDescription> subpasses{1};
        // 既定の依存関係は、同じキューの前後の処理を同期する。
        std::vector<SubpassDependency> dependencies{
            SubpassDependency{.source_stage = PipelineStage::ALL_COMMANDS}, SubpassDependency{.source = 0, .destination = EXTERNAL_SUBPASS, .destination_stage = PipelineStage::ALL_COMMANDS}};
        AttachmentId add_attachment(AttachmentDescription attachment);
        AttachmentId add_color(AttachmentDescription attachment);
        AttachmentId set_depth_stencil(AttachmentDescription attachment);
        void validate() const;
    };

    struct ClearColor {
        float r = 0, g = 0, b = 0, a = 0;
    };
    struct ClearInt {
        std::int32_t r = 0, g = 0, b = 0, a = 0;
    };
    struct ClearUint {
        std::uint32_t r = 0, g = 0, b = 0, a = 0;
    };
    struct ClearDepthStencil {
        float depth = 1;
        std::uint32_t stencil = 0;
    };
    using ClearValue = std::variant<ClearColor, ClearInt, ClearUint, ClearDepthStencil>;
    struct AttachmentClear {
        AttachmentId attachment;
        ClearValue value;
    };

    struct RenderPassAccessor {
        std::shared_ptr<impl::RenderPassImpl> data_;
        const RenderPassDescription& description() const;
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
