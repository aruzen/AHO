#pragma once
#include "../define.hpp"
#include "../exceptions.hpp"
#include "pv.hpp"
#include "device.hpp"
#include "shader.hpp"

namespace VSL_NAMESPACE {
    struct PipelineLayoutAccessor {
        std::shared_ptr<impl::PipelineLayoutImpl> data_;
    };

    struct PipelineLayoutEditAccessor : PipelineLayoutAccessor {
    protected:
        void init_start(LogicalDeviceAccessor device);
        void init_finish();
    };

    template<typename T>
    concept pipeline_layout_injecter = requires(T&& t) {
        std::forward<T>(t).injection(std::declval<PipelineLayoutEditAccessor>());
    };

    class FrozenPipelineLayout : public PipelineLayoutAccessor {
        friend struct PipelineLayout;
        explicit FrozenPipelineLayout(std::shared_ptr<impl::PipelineLayoutImpl> data);
    };

    struct PipelineLayout : PipelineLayoutEditAccessor {
        template<pipeline_layout_injecter... Args>
        PipelineLayout(LogicalDeviceAccessor device, Args&&... args);

        template<pipeline_layout_injecter... Additions>
        PipelineLayout add(Additions&&... additions);

        PipelineLayout copy() const;
        // 編集用の値を消費し、以降はPipeline生成とコマンドからの参照に使う。
        FrozenPipelineLayout freeze() &&;

    private:
        explicit PipelineLayout(std::shared_ptr<impl::PipelineLayoutImpl> data);
        PipelineLayout clone_state() const;
    };

    namespace helper {
        void expansion_pipeline_layout_args(PipelineLayoutEditAccessor&);

        template<pipeline_layout_injecter T, typename... Args>
        void expansion_pipeline_layout_args(PipelineLayoutEditAccessor& layout, T&& stage, Args&&... args);
    } // namespace helper

    inline FrozenPipelineLayout::FrozenPipelineLayout(std::shared_ptr<impl::PipelineLayoutImpl> data)
        : PipelineLayoutAccessor{std::move(data)} {}

    namespace helper {
        inline void expansion_pipeline_layout_args(PipelineLayoutEditAccessor&) {}

        template<pipeline_layout_injecter T, typename... Args>
        void expansion_pipeline_layout_args(PipelineLayoutEditAccessor& layout, T&& stage, Args&&... args) {
            std::forward<T>(stage).injection(layout);
            expansion_pipeline_layout_args(layout, std::forward<Args>(args)...);
        }
    } // namespace helper

    template<pipeline_layout_injecter... Args>
    PipelineLayout::PipelineLayout(LogicalDeviceAccessor device, Args&&... args) {
        init_start(device);
        helper::expansion_pipeline_layout_args(*this, std::forward<Args>(args)...);
        init_finish();
    }

    template<pipeline_layout_injecter... Additions>
    PipelineLayout PipelineLayout::add(Additions&&... additions) {
        auto next = clone_state();
        helper::expansion_pipeline_layout_args(next, std::forward<Additions>(additions)...);
        next.init_finish();
        data_ = std::move(next.data_);
        return *this;
    }
} // namespace VSL_NAMESPACE
