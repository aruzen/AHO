#pragma once

#include "../define.hpp"
#include "../lifetime/resource_identity.hpp"
#include "pv.hpp"

#include "pipeline_layout.hpp"
#include "render_pass.hpp"
#include "command.hpp"
#include <span>

namespace VSL_NAMESPACE::lifetime::detail {
    struct GraphicsAccess;
}

namespace VSL_NAMESPACE {
    struct PipelineAccessor {
    private:
        friend struct lifetime::detail::GraphicsAccess;
        lifetime::OwnedRef<impl::PipelineImpl> _owner;
        explicit PipelineAccessor(lifetime::OwnedRef<impl::PipelineImpl> owner) : _owner(std::move(owner)) {}

    public:
        PipelineAccessor() = default;
        PipelineAccessor(const PipelineAccessor&) = default;
        PipelineAccessor(PipelineAccessor&&) noexcept = default;
        PipelineAccessor& operator=(PipelineAccessor replacement) noexcept {
            swap(replacement);
            return *this;
        }
        explicit operator bool() const noexcept { return bool(_owner); }
        void swap(PipelineAccessor& other) noexcept { _owner.swap(other._owner); }
        PipelineIdentity identity() const;
        LogicalDeviceAccessor get_device() const;
        lifetime::OwnerRef owner_ref() const;
    };

    struct GraphicsPipeline {
    private:
        friend struct lifetime::detail::GraphicsAccess;
        PipelineAccessor _value;

    public:
        GraphicsPipeline(const GraphicsPipeline&) = default;
        GraphicsPipeline(GraphicsPipeline&&) noexcept = default;
        GraphicsPipeline& operator=(GraphicsPipeline replacement) noexcept {
            swap(replacement);
            return *this;
        }
        void swap(GraphicsPipeline& other) noexcept { _value.swap(other._value); }
        operator PipelineAccessor() const { return _value; }
        explicit operator bool() const noexcept { return bool(_value); }
        PipelineIdentity identity() const { return _value.identity(); }
        LogicalDeviceAccessor get_device() const { return _value.get_device(); }
        lifetime::OwnerRef owner_ref() const { return _value.owner_ref(); }
        GraphicsPipeline(PipelineLayoutAccessor layout, RenderPassAccessor pass, std::uint32_t subpass = 0);
        GraphicsPipeline(PipelineLayoutAccessor layout, RenderPassAccessor pass, const GraphicsPipelineState& state,
                         std::span<const ShaderAccessor> shaders, std::uint32_t subpass = 0);
        explicit GraphicsPipeline(PipelineAccessor source);

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };

    struct ComputePipeline {
    private:
        friend struct lifetime::detail::GraphicsAccess;
        PipelineAccessor _value;

    public:
        ComputePipeline(const ComputePipeline&) = default;
        ComputePipeline(ComputePipeline&&) noexcept = default;
        ComputePipeline& operator=(ComputePipeline replacement) noexcept {
            swap(replacement);
            return *this;
        }
        void swap(ComputePipeline& other) noexcept { _value.swap(other._value); }
        operator PipelineAccessor() const { return _value; }
        explicit operator bool() const noexcept { return bool(_value); }
        PipelineIdentity identity() const { return _value.identity(); }
        LogicalDeviceAccessor get_device() const { return _value.get_device(); }
        lifetime::OwnerRef owner_ref() const { return _value.owner_ref(); }
        ComputePipeline(PipelineLayoutAccessor layout);
        ComputePipeline(PipelineLayoutAccessor layout, ShaderAccessor shader);
        explicit ComputePipeline(PipelineAccessor source);

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };
} // namespace VSL_NAMESPACE
