#include <VSL/lifetime/command_access.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/lifetime/resource_arena.hpp>
#include <VSL/lifetime/synchronization_storage.hpp>
#include <VSL/lifetime/dependency_node.hpp>
#include <VSL/utils/typed_frame_resources.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <limits>

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace VSL_NAMESPACE::typed::detail {
    FrameResourceDomain::FrameResourceDomain(CommandManager manager) : manager(std::move(manager)), count(0) {
        if (!CommandAccess::Data(this->manager) || !CommandAccess::Data(this->manager).use_count())
            throw std::invalid_argument("Typed frame resources require a command manager owner");
        this->manager.get_device();
        buffer = this->manager.get_buffer();
        if (!CommandAccess::Data(buffer) || !buffer.get_size())
            throw std::invalid_argument("Typed frame resources require command buffer slots");
        (void)buffer.owner_ref();
        if (buffer.get_pool().get_device() != this->manager.get_device())
            throw std::invalid_argument("Typed frame resources require their command device");
        count = buffer.get_size();
        if (count > std::numeric_limits<std::uint32_t>::max() || buffer.get_current_buffer_idx() >= count)
            throw std::invalid_argument("Typed frame resources require a valid uint32 slot range");
        require_current();
    }

    void FrameResourceDomain::require_current() const {
        if (!CommandAccess::Data(manager) || !CommandAccess::Data(manager).use_count() || !CommandAccess::Data(buffer) || CommandAccess::Data(manager)->command_buffer != CommandAccess::Data(buffer) ||
            CommandAccess::Data(manager)->command_buffer.owner_before(CommandAccess::Data(buffer)) || CommandAccess::Data(buffer).owner_before(CommandAccess::Data(manager)->command_buffer) ||
            CommandAccess::Data(manager)->command_pool.get() != CommandAccess::Data(buffer)->command_pool ||
            CommandAccess::Data(buffer)->command_buffers.size() != count || CommandAccess::Data(buffer)->current_buffer_idx >= count)
            throw std::invalid_argument("Typed frame commands require their original buffer ring");
        const auto& pool = CommandAccess::Data(manager)->command_pool;
        const auto* ring_node = CommandAccess::Data(buffer)->owner_node;
        const auto* pool_node = pool ? pool->owner_node : nullptr;
        if (!ring_node || ring_node->retiring() || !ring_node->matches(CommandAccess::Data(buffer)) ||
            !pool_node || pool_node->retiring() || !pool_node->matches(pool))
            throw std::invalid_argument("Typed frame commands require their canonical ring and pool owners");
    }

    FrameResourceAccess FrameResourceDomain::access(const ComputePhase& phase) const {
        if (!CommandAccess::Data(manager) || CommandAccess::Data(phase.manager) != CommandAccess::Data(manager) || CommandAccess::Data(manager)->command_buffer != CommandAccess::Data(buffer))
            throw std::invalid_argument("Typed frame resources belong to a different command manager or buffer ring");
        require_current();
        const auto completion = phase.completion();
        if (phase.state != ComputePhase::State::WAIT_SUBMIT || !completion ||
            completion.status() != Completion::Status::PENDING || !CommandAccess::Data(buffer)->recording_context || !CommandAccess::Data(buffer)->recording_context->recording ||
            (!CommandAccess::Data(buffer)->recording_context->work || CommandAccess::Data(buffer)->recording_context->work->result != completion.data_) ||
            phase.frame_slot != CommandAccess::Data(buffer)->current_buffer_idx ||
            phase.frame_slot >= count || CommandAccess::Data(buffer)->command_buffers.size() != count)
            throw std::logic_error("Typed frame resources require the currently recording phase");
        return {phase.frame_slot, completion};
    }
} // namespace VSL_NAMESPACE::typed::detail

namespace VSL_NAMESPACE::typed {
    struct TypedFrameCommands::Resources {
        CommandManager manager;
        FenceHolder fence;
    };
    TypedFrameCommands::Resources TypedFrameCommands::MakeArena(LogicalDeviceAccessor device, std::uint32_t count) {
        if (!count)
            throw std::invalid_argument("Typed frame commands require a positive slot count");
        static constexpr std::array<ResourceRequest, 3> REQUESTS{{{{1}, storage::ResourceKind::COMMAND_POOL}, {{2}, storage::ResourceKind::COMMAND_BUFFER}, {{3}, storage::ResourceKind::FENCE}}};
        static constexpr ResourcePlan<3> PLAN(REQUESTS);
        const std::array<lifetime::ResourceInit, 3> inputs{CommandPoolCreateInfo{}, lifetime::CommandBufferInit{ResourceId{1}, count}, FenceCreateInfo{count, false}};
        lifetime::ResourceArena arena(device, PLAN, inputs);
        return {CommandManager::FromBuffer(arena.command_buffer({2})), arena.fence({3})};
    }
    TypedFrameCommands::Resources TypedFrameCommands::MakeHeap(LogicalDeviceAccessor device, std::uint32_t count) {
        if (!count)
            throw std::invalid_argument("Typed frame commands require a positive slot count");
        CommandPool pool(device);
        CommandBuffer buffer(pool, count);
        return {CommandManager::FromBuffer(buffer), {lifetime::make_heap_fence(device, {count, false})}};
    }
    TypedFrameCommands::TypedFrameCommands(Resources resources) : TypedFrameCommands(std::move(resources.manager), std::move(resources.fence)) {}
    TypedFrameCommands::TypedFrameCommands(LogicalDeviceAccessor device, std::uint32_t count) : TypedFrameCommands(std::move(device), count, DefaultStorage{}) {}
    TypedFrameCommands::TypedFrameCommands(LogicalDeviceAccessor device, std::uint32_t count, ArenaStorage<>) : TypedFrameCommands(MakeArena(device, count)) {}
    TypedFrameCommands::TypedFrameCommands(LogicalDeviceAccessor device, std::uint32_t count, HeapStorage) : TypedFrameCommands(MakeHeap(device, count)) {}
    TypedFrameCommands::TypedFrameCommands(CommandManager manager, FenceHolder fence) : _domain(std::move(manager)), _fence(std::move(fence)) {
        if (_fence.size() < _domain.count || _fence.get_device() != _domain.manager.get_device())
            throw std::invalid_argument("Typed frame commands require a fence for every slot on the same device");
    }
    TypedFrameCommands& TypedFrameCommands::operator=(TypedFrameCommands other) noexcept {
        swap(other);
        return *this;
    }
    void TypedFrameCommands::swap(TypedFrameCommands& other) noexcept {
        std::swap(_domain, other._domain);
        std::swap(_fence, other._fence);
    }
    size_t TypedFrameCommands::frame_count() const {
        _domain.require_current();
        return _domain.count;
    }
    CommandManager TypedFrameCommands::manager() const {
        _domain.require_current();
        return _domain.manager;
    }
    FenceHolder TypedFrameCommands::fence() const {
        _domain.require_current();
        return _fence;
    }
    ComputePhase TypedFrameCommands::start_compute() const {
        _domain.require_current();
        return ComputePhase(_domain.manager, std::nullopt, std::nullopt, _fence);
    }
    void TypedFrameCommands::next() {
        _domain.require_current();
        _domain.buffer.next();
    }
} // namespace VSL_NAMESPACE::typed
