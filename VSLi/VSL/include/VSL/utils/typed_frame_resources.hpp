#pragma once
#include "../vulkan/phase.hpp"
#include <concepts>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace VSL_NAMESPACE::typed {
    namespace detail {
        struct FrameResourceAccess {
            size_t slot;
            Completion completion;
        };
        struct FrameResourceDomain {
            CommandManager manager;
            CommandBuffer buffer;
            size_t count;
            explicit FrameResourceDomain(CommandManager manager);
            FrameResourceAccess access(const ComputePhase& phase) const;
        };
    } // namespace detail

    // Frameにはアプリ側で型付きbuffer・set等の所有者をまとめる。
    // 各記録Phaseでは、get()より前に一度だけ更新できる。
    template<typename Frame_>
    class TypedFrameResources {
        static_assert(std::is_object_v<Frame_> && !std::is_const_v<Frame_> && !std::is_volatile_v<Frame_>);
        struct Slot {
            std::shared_ptr<Frame_> value;
            Completion completion;
            bool updating = false;
            bool failed = false;
        };
        detail::FrameResourceDomain _domain;
        std::vector<Slot> _slots;
        Slot& acquire(const ComputePhase& phase, bool update);

    public:
        using Frame = Frame_;
        template<typename Factory>
        requires std::invocable<Factory&, size_t> && std::same_as<std::invoke_result_t<Factory&, size_t>, Frame_>
        TypedFrameResources(CommandManager manager, Factory&& factory);
        TypedFrameResources(const TypedFrameResources&) = delete;
        TypedFrameResources& operator=(const TypedFrameResources&) = delete;
        TypedFrameResources(TypedFrameResources&&) noexcept = default;
        TypedFrameResources& operator=(TypedFrameResources&&) noexcept = default;
        size_t frame_count() const { return _slots.size(); }
        template<typename Update>
        requires std::invocable<Update&, Frame_&> && std::same_as<std::invoke_result_t<Update&, Frame_&>, void>
        void update(const ComputePhase& phase, Update&& update);
        const Frame& get(const ComputePhase& phase);
    };

    template<typename Frame_>
    template<typename Factory>
    requires std::invocable<Factory&, size_t> && std::same_as<std::invoke_result_t<Factory&, size_t>, Frame_>
    TypedFrameResources<Frame_>::TypedFrameResources(CommandManager manager, Factory&& factory)
        : _domain(std::move(manager)) {
        _slots.reserve(_domain.count);
        for (size_t index = 0; index < _domain.count; ++index)
            _slots.push_back({std::make_shared<Frame>(std::invoke(factory, index)), {}});
    }

    template<typename Frame>
    typename TypedFrameResources<Frame>::Slot& TypedFrameResources<Frame>::acquire(const ComputePhase& phase, bool update) {
        const auto access = _domain.access(phase);
        auto& slot = _slots.at(access.slot);
        if (slot.completion.data_ == access.completion.data_) {
            if (update || slot.updating || slot.failed)
                throw std::logic_error("Typed frame resources are already in use by this phase");
            return slot;
        }
        const auto status = slot.completion.status();
        if (status == Completion::Status::PENDING || status == Completion::Status::SUBMITTED)
            throw std::logic_error("Typed frame resources require a completed or cancelled previous phase");
        access.completion.retain(slot.value);
        slot.completion = access.completion;
        slot.failed = false;
        return slot;
    }

    template<typename Frame_>
    template<typename Update>
    requires std::invocable<Update&, Frame_&> && std::same_as<std::invoke_result_t<Update&, Frame_&>, void>
    void TypedFrameResources<Frame_>::update(const ComputePhase& phase, Update&& update) {
        auto& slot = acquire(phase, true);
        slot.updating = true;
        try {
            std::invoke(update, *slot.value);
        } catch (...) {
            slot.updating = false;
            slot.failed = true;
            throw;
        }
        slot.updating = false;
    }

    template<typename Frame>
    const Frame& TypedFrameResources<Frame>::get(const ComputePhase& phase) {
        return *acquire(phase, false).value;
    }
} // namespace VSL_NAMESPACE::typed
