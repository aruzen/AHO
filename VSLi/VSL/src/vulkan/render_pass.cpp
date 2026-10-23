#include <VSL/vulkan/render_pass.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/swapchain.hpp>
#include <VSL/vulkan/image.hpp>
#include <VSL/vulkan/commands/image_readback.hpp>
#include "image_format.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace VSL_NAMESPACE {
    AttachmentId RenderPassDescription::add_attachment(AttachmentDescription attachment) {
        if (attachments.size() >= UNUSED_ATTACHMENT)
            throw std::length_error("Too many attachments");
        attachments.push_back(attachment);
        return static_cast<AttachmentId>(attachments.size() - 1);
    }
    AttachmentId RenderPassDescription::add_color(AttachmentDescription attachment) {
        if (subpasses.empty())
            subpasses.emplace_back();
        auto id = add_attachment(attachment);
        subpasses.front().colors.push_back({id, ImageLayout::COLOR_ATTACHMENT_OPTIMAL});
        return id;
    }
    AttachmentId RenderPassDescription::set_depth_stencil(AttachmentDescription attachment) {
        if (subpasses.empty())
            subpasses.emplace_back();
        if (subpasses.front().depth_stencil)
            throw std::invalid_argument("Depth/stencil attachment already set");
        auto id = add_attachment(attachment);
        subpasses.front().depth_stencil = {id, ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        return id;
    }
    void RenderPassDescription::validate() const {
        if (subpasses.empty())
            throw std::invalid_argument("RenderPass needs at least one subpass");
        for (const auto& a : attachments) {
            const auto samples = static_cast<std::uint32_t>(a.samples);
            if (a.format.type() == data_format::FormatTypes::UNDEFINED || !samples || samples > 64 ||
                (samples & (samples - 1)))
                throw std::invalid_argument("Invalid attachment format/sample count");
            if (a.final_layout == ImageLayout::UNDEFINED || a.final_layout == ImageLayout::PREINITIALIZED)
                throw std::invalid_argument("Attachment needs a defined final layout");
            if ((a.load == LoadOp::LOAD || a.stencil_load == LoadOp::LOAD) &&
                a.initial_layout == ImageLayout::UNDEFINED)
                throw std::invalid_argument("Cannot load undefined attachment contents");
        }
        for (const auto& s : subpasses) {
            if (!s.resolves.empty() && s.resolves.size() != s.colors.size())
                throw std::invalid_argument("Resolve attachment count must match color output count");
            auto check = [&](AttachmentReference ref, bool depth) {
                if (ref.attachment == UNUSED_ATTACHMENT)
                    return;
                if (ref.attachment >= attachments.size())
                    throw std::out_of_range("Attachment reference");
                const bool is_depth = impl::helper::image_aspects(
                                          static_cast<vk::Format>(attachments[ref.attachment].format.type())) !=
                                      vk::ImageAspectFlagBits::eColor;
                if (is_depth != depth)
                    throw std::invalid_argument("Attachment role does not match format");
                const bool valid_layout = depth ? ref.layout == ImageLayout::GENERAL ||
                                                      ref.layout == ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
                                                      ref.layout == ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                                : ref.layout == ImageLayout::GENERAL ||
                                                      ref.layout == ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
                if (!valid_layout)
                    throw std::invalid_argument("Invalid layout for attachment role");
            };
            std::optional<SampleCount> raster_samples;
            for (auto ref : s.colors) {
                check(ref, false);
                if (ref.attachment == UNUSED_ATTACHMENT)
                    continue;
                auto samples = attachments[ref.attachment].samples;
                if (raster_samples && *raster_samples != samples)
                    throw std::invalid_argument("Color sample counts differ");
                raster_samples = samples;
            }
            if (s.depth_stencil && s.depth_stencil->attachment != UNUSED_ATTACHMENT) {
                check(*s.depth_stencil, true);
                if (raster_samples && *raster_samples != attachments[s.depth_stencil->attachment].samples)
                    throw std::invalid_argument("Depth/color sample counts differ");
            }
            for (auto ref : s.inputs) {
                if (ref.attachment == UNUSED_ATTACHMENT)
                    continue;
                if (ref.attachment >= attachments.size())
                    throw std::out_of_range("Input attachment reference");
                if (ref.layout != ImageLayout::GENERAL && ref.layout != ImageLayout::SHADER_READ_ONLY_OPTIMAL &&
                    ref.layout != ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL)
                    throw std::invalid_argument("Invalid input attachment layout");
            }
            for (std::size_t i = 0; i < s.resolves.size(); ++i) {
                auto ref = s.resolves[i];
                check(ref, false);
                if (ref.attachment == UNUSED_ATTACHMENT)
                    continue;
                const auto src = s.colors[i].attachment;
                if (src == UNUSED_ATTACHMENT || attachments[src].samples == SampleCount::ONE ||
                    attachments[ref.attachment].samples != SampleCount::ONE ||
                    attachments[src].format.type() != attachments[ref.attachment].format.type())
                    throw std::invalid_argument("Invalid resolve attachment");
            }
            for (auto id : s.preserve) {
                if (id >= attachments.size())
                    throw std::out_of_range("Preserve attachment reference");
                auto used = [id](auto ref) { return ref.attachment == id; };
                if (std::ranges::any_of(s.colors, used) || std::ranges::any_of(s.inputs, used) ||
                    std::ranges::any_of(s.resolves, used) || (s.depth_stencil && used(*s.depth_stencil)))
                    throw std::invalid_argument("Cannot preserve an attachment used by the same subpass");
            }
        }
        for (const auto& d : dependencies) {
            const auto non_graphics = PipelineStage::COMPUTE_SHADER | PipelineStage::TRANSFER | PipelineStage::HOST | PipelineStage::ALL_COMMANDS;
            if ((d.source != EXTERNAL_SUBPASS && static_cast<std::uint32_t>(d.source_stage & non_graphics)) ||
                (d.destination != EXTERNAL_SUBPASS && static_cast<std::uint32_t>(d.destination_stage & non_graphics)))
                throw std::invalid_argument("Internal subpass dependencies require graphics stages");
            if ((d.source != EXTERNAL_SUBPASS && d.source >= subpasses.size()) ||
                (d.destination != EXTERNAL_SUBPASS && d.destination >= subpasses.size()) ||
                (d.source == EXTERNAL_SUBPASS && d.destination == EXTERNAL_SUBPASS) ||
                (d.source != EXTERNAL_SUBPASS && d.destination != EXTERNAL_SUBPASS && d.source > d.destination) ||
                !static_cast<std::uint32_t>(d.source_stage) || !static_cast<std::uint32_t>(d.destination_stage))
                throw std::invalid_argument("Invalid subpass dependency");
        }
    }
    const RenderPassDescription& RenderPassAccessor::description() const {
        if (!data_ || !data_->description)
            throw std::logic_error("RenderPass has no description");
        return *data_->description;
    }
    RenderPass::RenderPass(LogicalDeviceAccessor device, RenderPassDescription desc) {
        desc.validate();
        if (!device.data_)
            throw std::invalid_argument("RenderPass requires a device");
        auto data = std::make_shared<impl::RenderPassImpl>();
        data->device = device.data_;
        data->description = std::make_shared<const RenderPassDescription>(std::move(desc));
        std::vector<vk::AttachmentDescription> attachments;
        for (const auto& a : data->description->attachments)
            attachments.push_back({.format = static_cast<vk::Format>(a.format.type()),
                                   .samples = static_cast<vk::SampleCountFlagBits>(a.samples),
                                   .loadOp = static_cast<vk::AttachmentLoadOp>(a.load),
                                   .storeOp = static_cast<vk::AttachmentStoreOp>(a.store),
                                   .stencilLoadOp = static_cast<vk::AttachmentLoadOp>(a.stencil_load),
                                   .stencilStoreOp = static_cast<vk::AttachmentStoreOp>(a.stencil_store),
                                   .initialLayout = static_cast<vk::ImageLayout>(a.initial_layout),
                                   .finalLayout = static_cast<vk::ImageLayout>(a.final_layout)});
        struct References {
            std::vector<vk::AttachmentReference> colors, inputs, resolves;
            vk::AttachmentReference depth;
        };
        std::vector<References> refs(data->description->subpasses.size());
        std::vector<vk::SubpassDescription> subpasses;
        for (std::size_t i = 0; i < refs.size(); ++i) {
            const auto& s = data->description->subpasses[i];
            auto& r = refs[i];
            auto convert = [](const auto& src, auto& dst) {
                for (auto ref : src)
                    dst.push_back(vk::AttachmentReference{.attachment = ref.attachment,
                                                          .layout = static_cast<vk::ImageLayout>(ref.layout)});
            };
            convert(s.colors, r.colors);
            convert(s.inputs, r.inputs);
            convert(s.resolves, r.resolves);
            if (s.depth_stencil)
                r.depth = {.attachment = s.depth_stencil->attachment,
                           .layout = static_cast<vk::ImageLayout>(s.depth_stencil->layout)};
            subpasses.push_back({.pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
                                 .inputAttachmentCount = static_cast<std::uint32_t>(r.inputs.size()),
                                 .pInputAttachments = r.inputs.data(),
                                 .colorAttachmentCount = static_cast<std::uint32_t>(r.colors.size()),
                                 .pColorAttachments = r.colors.data(),
                                 .pResolveAttachments = r.resolves.empty() ? nullptr : r.resolves.data(),
                                 .pDepthStencilAttachment = s.depth_stencil ? &r.depth : nullptr,
                                 .preserveAttachmentCount = static_cast<std::uint32_t>(s.preserve.size()),
                                 .pPreserveAttachments = s.preserve.data()});
        }
        std::vector<vk::SubpassDependency> dependencies;
        for (const auto& d : data->description->dependencies)
            dependencies.push_back(
                {.srcSubpass = d.source,
                 .dstSubpass = d.destination,
                 .srcStageMask = vk::PipelineStageFlags(static_cast<std::uint32_t>(d.source_stage)),
                 .dstStageMask = vk::PipelineStageFlags(static_cast<std::uint32_t>(d.destination_stage)),
                 .srcAccessMask = vk::AccessFlags(static_cast<std::uint32_t>(d.source_access)),
                 .dstAccessMask = vk::AccessFlags(static_cast<std::uint32_t>(d.destination_access)),
                 .dependencyFlags = d.by_region ? vk::DependencyFlagBits::eByRegion : vk::DependencyFlags{}});
        vk::RenderPassCreateInfo info{.attachmentCount = static_cast<std::uint32_t>(attachments.size()),
                                      .pAttachments = attachments.data(),
                                      .subpassCount = static_cast<std::uint32_t>(subpasses.size()),
                                      .pSubpasses = subpasses.data(),
                                      .dependencyCount = static_cast<std::uint32_t>(dependencies.size()),
                                      .pDependencies = dependencies.data()};
        data->render_pass = data->device->device.createRenderPass(info);
        data_ = std::move(data);
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
        : RenderPass(LogicalDeviceAccessor{swapchain.data_ ? swapchain.data_->device : nullptr},
                     present_description(swapchain, false)) {}
    IDPickingRenderPass::IDPickingRenderPass(SwapchainAccessor swapchain) {
        RenderPass pass(LogicalDeviceAccessor{swapchain.data_ ? swapchain.data_->device : nullptr},
                        present_description(swapchain, true));
        data_ = pass.data_;
        picking_buffer.resize(swapchain.images.size());
        recreate_buffer(swapchain.data_->extent.width, swapchain.data_->extent.height);
    }
    void IDPickingRenderPass::recreate_buffer(std::uint32_t width, std::uint32_t height) {
        for (auto& image : picking_buffer)
            image = Image<ImageType::COLOR_ATTACHMENT | ImageType::TRANSFER_SRC>(
                LogicalDeviceAccessor{data_->device}, width, height, data_format::UNSIGNED_INT);
    }
    std::uint32_t IDPickingRenderPass::read(CommandManager manager, FenceHolder in_flight, std::uint32_t x,
                                            std::uint32_t y) {
        if (!in_flight.wait())
            throw std::runtime_error("Picking render did not complete");
        const auto index = manager.data_->command_buffer->acquired_image_index.value_or(manager.get_current_buffer_idx());
        auto image = picking_buffer.at(index);
        (void)command::ImageReadback::Describe(image, ImageLayout::COLOR_ATTACHMENT_OPTIMAL, {x, y, 1, 1});
        StagingBuffer<MemoryType::TRANSFER_DESTINATION> destination(LogicalDeviceAccessor{data_->device}, sizeof(std::uint32_t));
        auto buffer = manager.make_exclusive_buffer(1);
        auto native = buffer.data_->command_buffers.front();
        auto device = data_->device->device;
        auto fence = device.createFenceUnique({});
        native.begin(vk::CommandBufferBeginInfo{});
        command::ImageReadback(&destination, image, ImageLayout::COLOR_ATTACHMENT_OPTIMAL, {x, y, 1, 1})
            .invoke(manager.get_pool(), buffer, manager);
        native.end();
        manager.data_->graphics_queue.submit(vk::SubmitInfo{.commandBufferCount = 1, .pCommandBuffers = &native}, *fence);
        if (device.waitForFences(*fence, true, UINT64_MAX) != vk::Result::eSuccess)
            throw std::runtime_error("Picking transfer did not complete");
        auto mapped = destination.data(sizeof(std::uint32_t));
        std::uint32_t id;
        std::memcpy(&id, mapped.data, sizeof(id));
        return id;
    }
    impl::RenderPassImpl::~RenderPassImpl() {
        if (device)
            device->device.destroyRenderPass(render_pass);
    }
} // namespace VSL_NAMESPACE
