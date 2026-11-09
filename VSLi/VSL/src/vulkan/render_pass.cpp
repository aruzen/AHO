#include <VSL/lifetime/graphics_access.hpp>
#include <VSL/lifetime/facade_call.hpp>
#include <VSL/lifetime/identity_access.hpp>
#include <VSL/lifetime/command_access.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/vulkan/render_pass.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/swapchain.hpp>
#include <VSL/vulkan/image.hpp>
#include <VSL/vulkan/commands/image_readback.hpp>
#include <VSL/vulkan/phase.hpp>
#include <VSL/lifetime/render_pass_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {
    using VSL_NAMESPACE::lifetime::detail::GraphicsAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace VSL_NAMESPACE {
    RenderPassIdentity RenderPassAccessor::identity() const {
        return lifetime::detail::IdentityAccess::From<RenderPassIdentity>(_owner.get());
    }
    const RenderPassDescription& RenderPassAccessor::description() const& {
        if (!_owner || !_owner->description)
            throw std::logic_error("RenderPass has no description");
        return *_owner->description;
    }
    LogicalDeviceAccessor RenderPassAccessor::get_device() const {
        if (!_owner || !_owner.use_count() || !_owner->render_pass || !_owner->device || !_owner->description)
            throw std::invalid_argument("Render pass requires a live owner");
        return lifetime::device_from_child(_owner);
    }
    lifetime::OwnerRef RenderPassAccessor::owner_ref() const {
        return _owner;
    }
    RenderPass::RenderPass(LogicalDeviceAccessor device, RenderPassDescription desc) {
        GraphicsAccess::Owned(*this) = GraphicsAccess::Owned(GraphicsAccess::FromShared(lifetime::make_heap_render_pass(std::move(device), std::move(desc))));
    }
    namespace {
        RenderPassDescription present_description(SwapchainAccessor swapchain, bool picking) {
            if (!swapchain.data_)
                throw std::invalid_argument("Swapchain required");
            RenderPassDescription desc;
            desc.add_color({.format = swapchain.get_format(),
                            .final_layout = ImageLayout::PRESENT});
            if (picking)
                desc.add_color({.format = data_format::UNSIGNED_INT});
            return desc;
        }
    } // namespace
    RenderPass::RenderPass(SwapchainAccessor swapchain)
        : RenderPass(swapchain.get_device(),
                     present_description(swapchain, false)) {}
    IDPickingRenderPass::IDPickingRenderPass(SwapchainAccessor swapchain) {
        RenderPass pass(swapchain.get_device(),
                        present_description(swapchain, true));
        _pass = std::move(pass);
        _picking_buffer.resize(swapchain.images.size());
        recreate_buffer(swapchain.data_->extent.width, swapchain.data_->extent.height);
    }
    IDPickingRenderPass::IDPickingRenderPass(IDPickingRenderPass&& other) noexcept {
        lifetime::FacadeCall::Invalidate(&other);
        _pass.swap(other._pass);
        _picking_buffer.swap(other._picking_buffer);
    }
    IDPickingRenderPass& IDPickingRenderPass::operator=(IDPickingRenderPass replacement) noexcept {
        swap(replacement);
        return *this;
    }
    IDPickingRenderPass::~IDPickingRenderPass() {
        lifetime::FacadeCall::Invalidate(this);
    }
    void IDPickingRenderPass::swap(IDPickingRenderPass& other) noexcept {
        lifetime::FacadeCall::Invalidate(this);
        lifetime::FacadeCall::Invalidate(&other);
        _pass.swap(other._pass);
        _picking_buffer.swap(other._picking_buffer);
    }
    void IDPickingRenderPass::recreate_buffer(std::uint32_t width, std::uint32_t height) {
        lifetime::FacadeCall::Invalidate(this);
        lifetime::FacadeCall call(this);
        const auto device = get_device();
        const auto count = _picking_buffer.size();
        std::vector<ImageAccessor> next;
        next.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            next.emplace_back(Image<ImageType::COLOR_ATTACHMENT | ImageType::TRANSFER_SRC>(device, width, height, data_format::UNSIGNED_INT));
            if (!call.alive())
                throw std::logic_error("Picking image recreation was invalidated");
        }
        _picking_buffer.swap(next);
    }
    std::uint32_t IDPickingRenderPass::read(CommandManager manager, FenceHolder in_flight, std::uint32_t x,
                                            std::uint32_t y) {
        auto buffer = manager.get_buffer();
        const auto data = CommandAccess::Data(buffer);
        if (!data || !data->recording_context)
            throw std::logic_error("Picking read requires a command recording context");
        const auto index = data->recording_context->acquired_image_index.value_or(buffer.get_current_buffer_idx());
        auto image = _picking_buffer.at(index);
        const auto device = get_device();
        if (!in_flight.wait())
            throw std::runtime_error("Picking render did not complete");
        (void)command::ImageReadback::Describe(image, ImageLayout::COLOR_ATTACHMENT_OPTIMAL, {x, y, 1, 1});
        StagingBuffer<MemoryType::TRANSFER_DESTINATION> destination(device, sizeof(std::uint32_t));
        auto transfer = manager.independent(1);
        auto phase = transfer.start_phase<ComputePhase>();
        phase << command::ImageReadback(&destination, image, ImageLayout::COLOR_ATTACHMENT_OPTIMAL, {x, y, 1, 1});
        if (phase.submit() != ComputePhase::SubmitResult::SUCCESS || !phase.completion().wait())
            throw std::runtime_error("Picking transfer did not complete");
        std::uint32_t id;
        destination.read_into(std::as_writable_bytes(std::span(&id, 1)));
        return id;
    }
} // namespace VSL_NAMESPACE

VSL_NAMESPACE::RenderPassAccessor VSL_NAMESPACE::lifetime::detail::GraphicsAccess::FromShared(std::shared_ptr<impl::RenderPassImpl> owner) {
    const auto* node = owner ? owner->owner_node : nullptr;
    return FromOwned(owned_ref(std::move(owner), node));
}
