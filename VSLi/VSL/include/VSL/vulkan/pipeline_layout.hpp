#pragma once
#include "../define.hpp"
#include "../exceptions.hpp"
#include "../lifetime/owner_ref.hpp"
#include "pv.hpp"
#include "device.hpp"
#include "shader.hpp"

namespace VSL_NAMESPACE {
    struct PipelineLayoutAccessor {
        std::shared_ptr<impl::PipelineLayoutImpl> data_;
        PipelineLayoutAccessor() = default;
        PipelineLayoutAccessor(const PipelineLayoutAccessor&) = default;
        PipelineLayoutAccessor(PipelineLayoutAccessor&&) noexcept = default;
        explicit PipelineLayoutAccessor(std::shared_ptr<impl::PipelineLayoutImpl> data) : data_(std::move(data)) {}
        PipelineLayoutAccessor& operator=(PipelineLayoutAccessor other) noexcept;
        PipelineLayoutAccessor runtime() const { return PipelineLayoutAccessor(data_); }
        LogicalDeviceAccessor get_device() const;
        lifetime::OwnerRef owner_ref() const;
        bool has_recipe() const { return bool(_recipe); }
        const impl::PipelineRecipe& recipe() const&;
        const impl::PipelineRecipe& recipe() const&& = delete;

    private:
        friend struct PipelineLayoutEditAccessor;
        friend struct PipelineLayout;
        std::shared_ptr<const impl::PipelineRecipe> _recipe;
        void swap_state(PipelineLayoutAccessor& other) noexcept;
    };

    struct PipelineLayoutEditAccessor : PipelineLayoutAccessor {
        PipelineLayoutEditAccessor(const PipelineLayoutEditAccessor&) = default;
        PipelineLayoutEditAccessor(PipelineLayoutEditAccessor&&) noexcept = default;
        PipelineLayoutEditAccessor& operator=(PipelineLayoutEditAccessor other) noexcept;
        impl::PipelineRecipe& edit_recipe() const&;
        impl::PipelineRecipe& edit_recipe() const&& = delete;
        LogicalDeviceAccessor get_device() const;

    protected:
        PipelineLayoutEditAccessor() = default;
        void init_start(LogicalDeviceAccessor device);
        void init_finish(PipelineLayoutAccessor runtime = {});
        void swap_state(PipelineLayoutEditAccessor& other) noexcept;

    private:
        friend struct PipelineLayout;
        std::shared_ptr<impl::PipelineRecipe> _edit_recipe;
        LogicalDeviceAccessor _edit_device;
    };

    template<typename T>
    concept pipeline_layout_injecter = requires(T&& t) {
        std::forward<T>(t).injection(std::declval<PipelineLayoutEditAccessor>());
    };

    class FrozenPipelineLayout : public PipelineLayoutAccessor {
        friend struct PipelineLayout;
        explicit FrozenPipelineLayout(PipelineLayoutAccessor value) : PipelineLayoutAccessor(std::move(value)) {}
    };

    struct PipelineLayout : PipelineLayoutEditAccessor {
        template<pipeline_layout_injecter... Args>
        PipelineLayout(LogicalDeviceAccessor device, Args&&... args);
        template<pipeline_layout_injecter... Args>
        static PipelineLayout FromRuntime(PipelineLayoutAccessor runtime, Args&&... args);
        template<pipeline_layout_injecter... Additions>
        PipelineLayout add(Additions&&... additions);
        PipelineLayout copy() const;
        FrozenPipelineLayout freeze() &&;

    private:
        PipelineLayout() = default;
        PipelineLayout clone_state() const;
    };

    namespace helper {
        void expansion_pipeline_layout_args(PipelineLayoutEditAccessor&);
        template<pipeline_layout_injecter T, typename... Args>
        void expansion_pipeline_layout_args(PipelineLayoutEditAccessor& layout, T&& stage, Args&&... args);
        inline void expansion_pipeline_layout_args(PipelineLayoutEditAccessor&) {}
        template<pipeline_layout_injecter T, typename... Args>
        void expansion_pipeline_layout_args(PipelineLayoutEditAccessor& layout, T&& stage, Args&&... args) {
            std::forward<T>(stage).injection(layout);
            expansion_pipeline_layout_args(layout, std::forward<Args>(args)...);
        }
    } // namespace helper

    inline PipelineLayoutAccessor& PipelineLayoutAccessor::operator=(PipelineLayoutAccessor other) noexcept {
        swap_state(other);
        return *this;
    }
    inline void PipelineLayoutAccessor::swap_state(PipelineLayoutAccessor& other) noexcept {
        data_.swap(other.data_);
        _recipe.swap(other._recipe);
    }
    inline PipelineLayoutEditAccessor& PipelineLayoutEditAccessor::operator=(PipelineLayoutEditAccessor other) noexcept {
        swap_state(other);
        return *this;
    }

    template<pipeline_layout_injecter... Args>
    PipelineLayout::PipelineLayout(LogicalDeviceAccessor device, Args&&... args) {
        init_start(device);
        helper::expansion_pipeline_layout_args(*this, std::forward<Args>(args)...);
        init_finish();
    }
    template<pipeline_layout_injecter... Args>
    PipelineLayout PipelineLayout::FromRuntime(PipelineLayoutAccessor runtime, Args&&... args) {
        PipelineLayout result;
        result.init_start(runtime.get_device());
        helper::expansion_pipeline_layout_args(result, std::forward<Args>(args)...);
        result.init_finish(std::move(runtime));
        return result;
    }
    template<pipeline_layout_injecter... Additions>
    PipelineLayout PipelineLayout::add(Additions&&... additions) {
        auto next = clone_state();
        helper::expansion_pipeline_layout_args(next, std::forward<Additions>(additions)...);
        next.init_finish();
        swap_state(next);
        return *this;
    }
} // namespace VSL_NAMESPACE
