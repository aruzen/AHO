#include <VSL/operation/graphics_pipeline_state.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE {
    impl::GraphicsPipelineStateData& GraphicsPipelineStateEditRef::get() const {
        if (!data)
            throw std::logic_error("Graphics state edit requires a live borrowed value");
        return *data;
    }
    GraphicsPipelineState::GraphicsPipelineState() : _data(std::make_unique<impl::GraphicsPipelineStateData>()) {}
    GraphicsPipelineState::GraphicsPipelineState(const GraphicsPipelineState& other)
        : _data(std::make_unique<impl::GraphicsPipelineStateData>(other.definition())) {}
    GraphicsPipelineState::GraphicsPipelineState(GraphicsPipelineState&& other) noexcept = default;
    GraphicsPipelineState& GraphicsPipelineState::operator=(const GraphicsPipelineState& other) {
        auto next = std::make_unique<impl::GraphicsPipelineStateData>(other.definition());
        _data.swap(next);
        return *this;
    }
    GraphicsPipelineState& GraphicsPipelineState::operator=(GraphicsPipelineState&& other) noexcept = default;
    GraphicsPipelineState::~GraphicsPipelineState() = default;
    GraphicsPipelineStateEditRef GraphicsPipelineState::edit() & {
        if (!_data)
            throw std::logic_error("Graphics state has been moved");
        return {_data.get()};
    }
    const impl::GraphicsPipelineStateData& GraphicsPipelineState::definition() const& {
        if (!_data)
            throw std::logic_error("Graphics state has been moved");
        return *_data;
    }
} // namespace VSL_NAMESPACE
