#include <VSL/vulkan/frame_buffer.hpp>
#include <VSL/lifetime/resource_arena.hpp>

namespace t = vsl::typed;
namespace s = vsl::storage;
namespace l = vsl::lifetime;
constexpr std::array<t::ResourceRequest, 2> REQUESTS{{{{1}, s::ResourceKind::FRAME_BUFFER}, {{2}, s::ResourceKind::FRAME_BUFFER}}};
constexpr t::ResourcePlan<2> PLAN(REQUESTS);
static_assert(PLAN.bytes() == 2 * s::FRAME_BUFFER_SLOT_SIZE);
static_assert(PLAN.placement({2}).offset == s::FRAME_BUFFER_SLOT_SIZE);
static_assert(s::FRAME_BUFFER_SLOT_SIZE >= s::FRAME_BUFFER_IMPL_SIZE);
void connect_framebuffers(vsl::LogicalDeviceAccessor device, vsl::RenderPassAccessor pass, vsl::ImageAccessor image);
void connect_framebuffers(vsl::LogicalDeviceAccessor device, vsl::RenderPassAccessor pass, vsl::ImageAccessor image) {
    const std::array<l::ResourceInit, 2> inputs{l::FrameBufferInit{pass, 1, 1, 1, {{image}, {image}}}, l::FrameBufferInit{pass, 1, 1, 1, {{image}}}};
    l::ResourceArena arena(device, PLAN, inputs);
    const auto target = arena.frame_buffer({1});
    const auto original = target.get_render_pass();
    (void)target.get_device();
    (void)target.frame_count();
    (void)target.owner_ref();
}
