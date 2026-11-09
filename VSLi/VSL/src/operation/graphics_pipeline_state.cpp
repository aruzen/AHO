#include <VSL/operation/graphics_pipeline_state_access.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE {
    using operation::detail::GraphicsPipelineStateAccess;
    impl::GraphicsPipelineStateData& GraphicsPipelineStateAccess::Edit(GraphicsPipelineStateEditRef state) {
        if (!state._data)
            throw std::logic_error("Graphics state edit requires a live borrowed value");
        return *state._data;
    }
    GraphicsPipelineState::GraphicsPipelineState() : _data(std::make_unique<impl::GraphicsPipelineStateData>()) {}
    GraphicsPipelineState::GraphicsPipelineState(const GraphicsPipelineState& other)
        : _data(std::make_unique<impl::GraphicsPipelineStateData>(GraphicsPipelineStateAccess::Read(other))) {}
    GraphicsPipelineState::GraphicsPipelineState(GraphicsPipelineState&& other) noexcept = default;
    GraphicsPipelineState& GraphicsPipelineState::operator=(const GraphicsPipelineState& other) {
        auto next = std::make_unique<impl::GraphicsPipelineStateData>(GraphicsPipelineStateAccess::Read(other));
        _data.swap(next);
        return *this;
    }
    GraphicsPipelineState& GraphicsPipelineState::operator=(GraphicsPipelineState&& other) noexcept = default;
    GraphicsPipelineState::~GraphicsPipelineState() = default;
    GraphicsPipelineStateEditRef GraphicsPipelineState::edit() & {
        if (!_data)
            throw std::logic_error("Graphics state has been moved");
        return GraphicsPipelineStateAccess::Borrow(*_data);
    }
    const impl::GraphicsPipelineStateData& GraphicsPipelineStateAccess::Read(const GraphicsPipelineState& state) {
        if (!state._data)
            throw std::logic_error("Graphics state has been moved");
        return *state._data;
    }
} // namespace VSL_NAMESPACE
