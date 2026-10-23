#pragma once
#include "draw_list.hpp"
#include <VSL/vulkan/phase.hpp>
#include <VSL/vulkan/pipeline.hpp>
#include <filesystem>
#include <memory>
#include <optional>

namespace AHO_NAMESPACE::x {
    namespace command {
        class Draw2DCommand;
    }

    class Renderer2D {
        struct Data;
        std::shared_ptr<Data> _data;

    public:
        Renderer2D(VSL_NAMESPACE::LogicalDeviceAccessor device, VSL_NAMESPACE::RenderPassAccessor pass,
                   const std::filesystem::path& shader_directory, std::size_t max_snapshots = 3, std::uint32_t subpass = 0);
        // 空きスナップショットがなければnullopt。待機や追加のGPU送信はしない。
        [[nodiscard]] std::optional<command::Draw2DCommand> draw(VSL_NAMESPACE::ComputePhase& phase, const DrawList& list, std::uint32_t width, std::uint32_t height);
        [[nodiscard]] bool record(VSL_NAMESPACE::ComputePhase& phase, const DrawList& list, std::uint32_t width, std::uint32_t height);
        [[nodiscard]] std::size_t capacity() const noexcept;
    };
} // namespace AHO_NAMESPACE::x
