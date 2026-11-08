#pragma once
#include "../define.hpp"
#include <memory>
#include <utility>

namespace VSL_NAMESPACE {
    struct PipelineLayoutEditAccessor;
    namespace impl {
        struct GraphicsPipelineStateData;
    }
    // 注入呼出し中だけ使う借用。Device/native resource/ownerを保持しない。
    struct GraphicsPipelineStateEditRef {
        impl::GraphicsPipelineStateData* data = nullptr;
        impl::GraphicsPipelineStateData& get() const;
    };
    template<typename T>
    concept graphics_pipeline_state_injecter = requires(T&& value, GraphicsPipelineStateEditRef state) {
        std::forward<T>(value).injection(state);
    };
    // native生成から独立した入力値。コピーは内部配列を含めて独立する。
    class GraphicsPipelineState {
        std::unique_ptr<impl::GraphicsPipelineStateData> _data;

    public:
        GraphicsPipelineState();
        template<graphics_pipeline_state_injecter... Stages>
        requires(sizeof...(Stages) > 0)
        explicit GraphicsPipelineState(Stages&&... stages);
        GraphicsPipelineState(const GraphicsPipelineState& other);
        GraphicsPipelineState(GraphicsPipelineState&& other) noexcept;
        GraphicsPipelineState& operator=(const GraphicsPipelineState& other);
        GraphicsPipelineState& operator=(GraphicsPipelineState&& other) noexcept;
        ~GraphicsPipelineState();
        GraphicsPipelineStateEditRef edit() &;
        GraphicsPipelineStateEditRef edit() && = delete;
        const impl::GraphicsPipelineStateData& definition() const&;
        const impl::GraphicsPipelineStateData& definition() const&& = delete;
        void injection(PipelineLayoutEditAccessor layout) const;
    };

    template<graphics_pipeline_state_injecter... Stages>
    requires(sizeof...(Stages) > 0)
    GraphicsPipelineState::GraphicsPipelineState(Stages&&... stages) : GraphicsPipelineState() {
        (std::forward<Stages>(stages).injection(edit()), ...);
    }
} // namespace VSL_NAMESPACE
