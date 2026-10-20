#pragma once
#include "draw_list.hpp"
#include <VSL/vulkan/phase.hpp>
#include <VSL/vulkan/pipeline.hpp>
#include <filesystem>
#include <memory>
#include <optional>

namespace AHO_NAMESPACE::x {
    // RenderPassBegin/Endの間で、VSL/AHOの描画ストリームへ渡す。
    class Draw2DCommand {
        struct Data;
        std::shared_ptr<Data> data_;
        explicit Draw2DCommand(std::shared_ptr<Data> data) : data_(std::move(data)) {}
        friend class Renderer2D;

    public:
        void invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) const;
        [[nodiscard]] VSL_NAMESPACE::PipelineAccessor getPipeline() const;
    };
    class Renderer2D {
        struct Data;
        std::shared_ptr<Data> data_;

    public:
        Renderer2D(VSL_NAMESPACE::LogicalDeviceAccessor device, VSL_NAMESPACE::RenderPassAccessor pass,
                   const std::filesystem::path& shaderDirectory, std::size_t maxSnapshots = 3, std::uint32_t subpass = 0);
        // 空きスナップショットがなければnullopt。待機や追加のGPU送信はしない。
        [[nodiscard]] std::optional<Draw2DCommand> draw(VSL_NAMESPACE::ComputePhase& phase, const DrawList& list, std::uint32_t width, std::uint32_t height);
        [[nodiscard]] bool record(VSL_NAMESPACE::ComputePhase& phase, const DrawList& list, std::uint32_t width, std::uint32_t height);
        [[nodiscard]] std::size_t capacity() const noexcept;
    };
} // namespace AHO_NAMESPACE::x
