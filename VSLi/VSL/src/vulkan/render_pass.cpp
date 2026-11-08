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

namespace VSL_NAMESPACE {
    const RenderPassDescription& RenderPassAccessor::description() const& {
        if (!data_ || !data_->description)
            throw std::logic_error("RenderPass has no description");
        return *data_->description;
    }
    LogicalDeviceAccessor RenderPassAccessor::get_device() const {
        if (!data_ || !data_.use_count() || !data_->render_pass || !data_->device || !data_->description)
            throw std::invalid_argument("Render pass requires a live owner");
        return lifetime::device_from_child(data_);
    }
    lifetime::OwnerRef RenderPassAccessor::owner_ref() const {
        return lifetime::detail::owned_ref(data_, data_ ? data_->owner_node : nullptr);
    }
    RenderPass::RenderPass(LogicalDeviceAccessor device, RenderPassDescription desc) {
        data_ = lifetime::make_heap_render_pass(std::move(device), std::move(desc));
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
        data_ = pass.data_;
        picking_buffer.resize(swapchain.images.size());
        recreate_buffer(swapchain.data_->extent.width, swapchain.data_->extent.height);
    }
    void IDPickingRenderPass::recreate_buffer(std::uint32_t width, std::uint32_t height) {
        for (auto& image : picking_buffer)
            image = Image<ImageType::COLOR_ATTACHMENT | ImageType::TRANSFER_SRC>(
                get_device(), width, height, data_format::UNSIGNED_INT);
    }
    std::uint32_t IDPickingRenderPass::read(CommandManager manager, FenceHolder in_flight, std::uint32_t x,
                                            std::uint32_t y) {
        if (!in_flight.wait())
            throw std::runtime_error("Picking render did not complete");
        const auto index = manager.data_->command_buffer->recording_context->acquired_image_index.value_or(manager.get_current_buffer_idx());
        auto image = picking_buffer.at(index);
        (void)command::ImageReadback::Describe(image, ImageLayout::COLOR_ATTACHMENT_OPTIMAL, {x, y, 1, 1});
        StagingBuffer<MemoryType::TRANSFER_DESTINATION> destination(get_device(), sizeof(std::uint32_t));
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
