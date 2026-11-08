#pragma once
#include "../vulkan/phase.hpp"
#include "../lifetime/resource_use.hpp"
#include "../storage/typed_storage_policy.hpp"
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
            void require_current() const;
            FrameResourceAccess access(const ComputePhase& phase) const;
        };
    } // namespace detail

    // 実行群の所有者。各Frame値へManager/Domainを入れず、データ領域と寿命を分ける。
    class TypedFrameCommands {
        struct Resources;
        detail::FrameResourceDomain _domain;
        FenceHolder _fence;
        explicit TypedFrameCommands(Resources resources);
        static Resources MakeArena(LogicalDeviceAccessor device, std::uint32_t count);
        static Resources MakeHeap(LogicalDeviceAccessor device, std::uint32_t count);

    public:
        TypedFrameCommands(LogicalDeviceAccessor device, std::uint32_t count, ArenaStorage<>);
        TypedFrameCommands(LogicalDeviceAccessor device, std::uint32_t count, HeapStorage);
        TypedFrameCommands(CommandManager manager, FenceHolder fence);
        TypedFrameCommands(const TypedFrameCommands&) = delete;
        TypedFrameCommands(TypedFrameCommands&&) noexcept = default;
        TypedFrameCommands& operator=(TypedFrameCommands other) noexcept;
        void swap(TypedFrameCommands& other) noexcept;
        size_t frame_count() const;
        CommandManager manager() const;
        FenceHolder fence() const;
        ComputePhase start_compute() const;
        void next();
    };

    // Frameにはアプリ側で型付きbuffer・set等の所有者をまとめる。
    // 各記録Phaseでは、get()より前に一度だけ更新できる。
    template<typename Frame_, ResourceRetention retention = ResourceRetention::COMPLETION>
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

    template<typename Frame_, ResourceRetention retention>
    template<typename Factory>
    requires std::invocable<Factory&, size_t> && std::same_as<std::invoke_result_t<Factory&, size_t>, Frame_>
    TypedFrameResources<Frame_, retention>::TypedFrameResources(CommandManager manager, Factory&& factory)
        : _domain(std::move(manager)) {
        _slots.reserve(_domain.count);
        for (size_t index = 0; index < _domain.count; ++index)
            _slots.push_back({std::make_shared<Frame>(std::invoke(factory, index)), {}});
    }

    template<typename Frame, ResourceRetention retention>
    typename TypedFrameResources<Frame, retention>::Slot& TypedFrameResources<Frame, retention>::acquire(const ComputePhase& phase, bool update) {
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
        if constexpr (retention == ResourceRetention::COMPLETION)
            _domain.buffer.retain_until_complete(slot.value);
        else
            access.completion.retain(slot.value);
        slot.completion = access.completion;
        slot.failed = false;
        return slot;
    }

    template<typename Frame_, ResourceRetention retention>
    template<typename Update>
    requires std::invocable<Update&, Frame_&> && std::same_as<std::invoke_result_t<Update&, Frame_&>, void>
    void TypedFrameResources<Frame_, retention>::update(const ComputePhase& phase, Update&& update) {
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

    template<typename Frame, ResourceRetention retention>
    const Frame& TypedFrameResources<Frame, retention>::get(const ComputePhase& phase) {
        return *acquire(phase, false).value;
    }
} // namespace VSL_NAMESPACE::typed
