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
    inline constexpr AttachmentId UnusedAttachment = UINT32_MAX;
    inline constexpr std::uint32_t ExternalSubpass = UINT32_MAX;

    enum class LoadOp {
        Load = 0,
        Clear = 1,
        DontCare = 2
    };
    enum class StoreOp {
        Store = 0,
        DontCare = 1
    };
    enum class PipelineStage : std::uint32_t {
        Top = 0x1,
        VertexShader = 0x8,
        FragmentShader = 0x80,
        EarlyDepthStencil = 0x100,
        LateDepthStencil = 0x200,
        ColorAttachmentOutput = 0x400,
        ComputeShader = 0x800,
        Transfer = 0x1000,
        Bottom = 0x2000,
        Host = 0x4000,
        AllGraphics = 0x8000,
        AllCommands = 0x10000
    };
    enum class AccessMask : std::uint32_t {
        None = 0,
        ShaderRead = 0x20,
        ShaderWrite = 0x40,
        ColorRead = 0x80,
        ColorWrite = 0x100,
        DepthStencilRead = 0x200,
        DepthStencilWrite = 0x400,
        TransferRead = 0x800,
        TransferWrite = 0x1000,
        HostRead = 0x2000,
        HostWrite = 0x4000,
        MemoryRead = 0x8000,
        MemoryWrite = 0x10000
    };
    ___VSL_EXPAND_FLAGS_OPERATOR(PipelineStage);
    ___VSL_EXPAND_FLAGS_OPERATOR(AccessMask);

    struct AttachmentDescription {
        data_format::___Format format = data_format::Srgb8RGBA;
        SampleCount samples = SampleCount::One;
        LoadOp load = LoadOp::Clear;
        StoreOp store = StoreOp::Store;
        LoadOp stencilLoad = LoadOp::DontCare;
        StoreOp stencilStore = StoreOp::DontCare;
        ImageLayout initialLayout = ImageLayout::Undefined;
        ImageLayout finalLayout = ImageLayout::ColorAttachmentOptimal;
    };
    struct AttachmentReference {
        AttachmentId attachment;
        ImageLayout layout = ImageLayout::ColorAttachmentOptimal;
    };
    struct SubpassDescription {
        // 配列内の位置がフラグメント出力のlocation、入力アタッチメントのインデックスに対応する。
        std::vector<AttachmentReference> colors;
        std::vector<AttachmentReference> inputs;
        std::vector<AttachmentReference> resolves;
        std::optional<AttachmentReference> depthStencil;
        std::vector<AttachmentId> preserve;
    };
    struct SubpassDependency {
        std::uint32_t source = ExternalSubpass;
        std::uint32_t destination = 0;
        PipelineStage sourceStage = PipelineStage::AllGraphics;
        PipelineStage destinationStage = PipelineStage::AllGraphics;
        AccessMask sourceAccess = AccessMask::MemoryWrite;
        AccessMask destinationAccess = AccessMask::MemoryRead | AccessMask::MemoryWrite;
        bool byRegion = false;
    };
    struct RenderPassDescription {
        std::vector<AttachmentDescription> attachments;
        std::vector<SubpassDescription> subpasses{1};
        // 既定の依存関係は、同じキューの前後の処理を同期する。
        std::vector<SubpassDependency> dependencies{
            SubpassDependency{.sourceStage = PipelineStage::AllCommands}, SubpassDependency{.source = 0, .destination = ExternalSubpass, .destinationStage = PipelineStage::AllCommands}};
        AttachmentId addAttachment(AttachmentDescription attachment);
        AttachmentId addColor(AttachmentDescription attachment);
        AttachmentId setDepthStencil(AttachmentDescription attachment);
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

    struct RenderPassAccessor {
        std::shared_ptr<_impl::RenderPass_impl> _data;
        const RenderPassDescription &description() const;
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
        std::uint32_t read(CommandManager manager, FenceHolder inFlight, std::uint32_t x, std::uint32_t y);
    };
}
