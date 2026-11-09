#pragma once
#include "../define.hpp"
#include "../exceptions.hpp"
#include "../lifetime/owner_ref.hpp"
#include "../operation/graphics_pipeline_state.hpp"
#include "pv.hpp"
#include "device.hpp"
#include "shader.hpp"

namespace VSL_NAMESPACE::lifetime::detail {
    struct GraphicsAccess;
    struct PipelineRecipeAccess;
} // namespace VSL_NAMESPACE::lifetime::detail

namespace VSL_NAMESPACE {
    struct PipelineLayoutAccessor {
        PipelineLayoutAccessor() = default;
        PipelineLayoutAccessor(const PipelineLayoutAccessor&) = default;
        PipelineLayoutAccessor(PipelineLayoutAccessor&&) noexcept = default;
        PipelineLayoutAccessor& operator=(PipelineLayoutAccessor other) noexcept;
        PipelineLayoutAccessor runtime() const { return PipelineLayoutAccessor(_owner); }
        explicit operator bool() const noexcept { return bool(_owner); }
        void swap(PipelineLayoutAccessor& other) noexcept { swap_state(other); }
        LogicalDeviceAccessor get_device() const;
        lifetime::OwnerRef owner_ref() const;
        bool has_recipe() const { return bool(_recipe); }

    private:
        friend struct lifetime::detail::GraphicsAccess;
        friend struct lifetime::detail::PipelineRecipeAccess;
        friend struct PipelineLayoutEditAccessor;
        lifetime::OwnedRef<impl::PipelineLayoutImpl> _owner;
        explicit PipelineLayoutAccessor(lifetime::OwnedRef<impl::PipelineLayoutImpl> owner) : _owner(std::move(owner)) {}
        friend struct PipelineLayout;
        std::shared_ptr<const impl::PipelineRecipe> _recipe;
        void swap_state(PipelineLayoutAccessor& other) noexcept;
    };

    struct PipelineLayoutEditAccessor {
        PipelineLayoutEditAccessor(const PipelineLayoutEditAccessor&) = default;
        PipelineLayoutEditAccessor(PipelineLayoutEditAccessor&&) noexcept = default;
        PipelineLayoutEditAccessor& operator=(PipelineLayoutEditAccessor other) noexcept;
        operator PipelineLayoutAccessor() const { return _value; }
        explicit operator bool() const noexcept { return bool(_value); }
        PipelineLayoutAccessor runtime() const { return _value.runtime(); }
        lifetime::OwnerRef owner_ref() const { return _value.owner_ref(); }
        bool has_recipe() const { return _value.has_recipe(); }
        GraphicsPipelineStateEditRef edit_state() const&;
        GraphicsPipelineStateEditRef edit_state() const&& = delete;
        LogicalDeviceAccessor get_device() const;

    protected:
        PipelineLayoutEditAccessor() = default;
        void init_start(LogicalDeviceAccessor device);
        void init_finish(PipelineLayoutAccessor runtime = {});
        void swap_state(PipelineLayoutEditAccessor& other) noexcept;

    private:
        friend struct lifetime::detail::GraphicsAccess;
        friend struct lifetime::detail::PipelineRecipeAccess;
        friend struct PipelineLayout;
        PipelineLayoutAccessor _value;
        std::shared_ptr<impl::PipelineRecipe> _edit_recipe;
        LogicalDeviceAccessor _edit_device;
    };

    template<typename T>
    concept pipeline_layout_injecter = requires(T&& t) {
        std::forward<T>(t).injection(std::declval<PipelineLayoutEditAccessor>());
    };

    class FrozenPipelineLayout {
        friend struct PipelineLayout;
        friend struct lifetime::detail::GraphicsAccess;
        friend struct lifetime::detail::PipelineRecipeAccess;
        PipelineLayoutAccessor _value;
        explicit FrozenPipelineLayout(PipelineLayoutAccessor value) : _value(std::move(value)) {}

    public:
        FrozenPipelineLayout(const FrozenPipelineLayout&) = default;
        FrozenPipelineLayout(FrozenPipelineLayout&&) noexcept = default;
        FrozenPipelineLayout& operator=(FrozenPipelineLayout replacement) noexcept {
            swap(replacement);
            return *this;
        }
        void swap(FrozenPipelineLayout& other) noexcept { _value.swap(other._value); }
        operator PipelineLayoutAccessor() const { return _value; }
        explicit operator bool() const noexcept { return bool(_value); }
        PipelineLayoutAccessor runtime() const { return _value.runtime(); }
        LogicalDeviceAccessor get_device() const { return _value.get_device(); }
        lifetime::OwnerRef owner_ref() const { return _value.owner_ref(); }
        bool has_recipe() const { return _value.has_recipe(); }
    };

    struct PipelineLayout : PipelineLayoutEditAccessor {
        PipelineLayout(const PipelineLayout&) = default;
        PipelineLayout(PipelineLayout&&) noexcept = default;
        PipelineLayout& operator=(PipelineLayout replacement) noexcept {
            swap_state(replacement);
            return *this;
        }
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
        _owner.swap(other._owner);
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
