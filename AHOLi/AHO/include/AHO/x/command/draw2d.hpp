#pragma once
#include "../../define.hpp"
#include <VSL/vulkan/command.hpp>
#include <VSL/vulkan/pipeline.hpp>
#include <memory>

namespace AHO_NAMESPACE::x {
    class Renderer2D;
}

namespace AHO_NAMESPACE::x::command {
    // RenderPassBegin/Endの間で、VSL/AHOの描画ストリームへ渡す。
    class Draw2DCommand {
        struct Data;
        std::shared_ptr<Data> _data;
        explicit Draw2DCommand(std::shared_ptr<Data> data);
        friend class AHO_NAMESPACE::x::Renderer2D;

    public:
        void invoke(VSL_NAMESPACE::CommandPool pool, VSL_NAMESPACE::CommandBuffer buffer, VSL_NAMESPACE::CommandManager manager) const;
        [[nodiscard]] VSL_NAMESPACE::PipelineAccessor getPipeline() const;
    };
} // namespace AHO_NAMESPACE::x::command
