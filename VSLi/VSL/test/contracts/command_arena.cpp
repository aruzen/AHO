#include <VSL/vulkan/command.hpp>
#include <VSL/lifetime/resource_arena.hpp>

namespace t = vsl::typed;
namespace l = vsl::lifetime;
namespace s = vsl::storage;
constexpr std::array<t::ResourceRequest, 3> REQUESTS{{{{1}, s::ResourceKind::COMMAND_BUFFER}, {{2}, s::ResourceKind::COMMAND_BUFFER}, {{3}, s::ResourceKind::COMMAND_POOL}}};
constexpr t::ResourcePlan<3> PLAN(REQUESTS);
static_assert(PLAN.placement({2}).offset == s::COMMAND_BUFFER_SLOT_SIZE);
static_assert(s::COMMAND_BUFFER_SLOT_SIZE > s::COMMAND_BUFFER_IMPL_SIZE);
static_assert(s::ResourceSlotLayout<vsl::impl::CommandBufferImpl>::USE_OFFSET == s::NO_SEPARATE_USE);
void connect_commands(vsl::LogicalDeviceAccessor device);
void connect_commands(vsl::LogicalDeviceAccessor device) {
    const std::array<l::ResourceInit, 3> initializers{l::CommandBufferInit{t::ResourceId{3}, 2},
                                                      l::CommandBufferInit{t::ResourceId{3}, 3}, vsl::CommandPoolCreateInfo{true}};
    l::ResourceArena arena(device, PLAN, initializers);
    auto first = arena.command_buffer({1});
    auto second = arena.command_buffer({2});
    auto manager = vsl::CommandManager::FromBuffer(first);
    (void)first.get_pool().get_device();
    (void)second.owner_ref();
    (void)arena.command_pool({3}).owner_ref();
    first.reset();
    second.next();
}
