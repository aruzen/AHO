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
    AttachmentId RenderPassDescription::addAttachment(AttachmentDescription attachment) {
        if (attachments.size() >= UnusedAttachment)
            throw std::length_error("Too many attachments");
        attachments.push_back(attachment);
        return static_cast<AttachmentId>(attachments.size() - 1);
    }
    AttachmentId RenderPassDescription::addColor(AttachmentDescription attachment) {
        if (subpasses.empty())
            subpasses.emplace_back();
        auto id = addAttachment(attachment);
        subpasses.front().colors.push_back({id, ImageLayout::ColorAttachmentOptimal});
        return id;
    }
    AttachmentId RenderPassDescription::setDepthStencil(AttachmentDescription attachment) {
        if (subpasses.empty())
            subpasses.emplace_back();
        if (subpasses.front().depthStencil)
            throw std::invalid_argument("Depth/stencil attachment already set");
        auto id = addAttachment(attachment);
        subpasses.front().depthStencil = {id, ImageLayout::DepthStencilAttachmentOptimal};
        return id;
    }
    void RenderPassDescription::validate() const {
        if (subpasses.empty())
            throw std::invalid_argument("RenderPass needs at least one subpass");
        for (const auto& a : attachments) {
            const auto samples = static_cast<std::uint32_t>(a.samples);
            if (a.format.type() == data_format::___FormatTypes::Undefined || !samples || samples > 64 ||
                (samples & (samples - 1)))
                throw std::invalid_argument("Invalid attachment format/sample count");
            if (a.finalLayout == ImageLayout::Undefined || a.finalLayout == ImageLayout::Preinitialized)
                throw std::invalid_argument("Attachment needs a defined final layout");
            if ((a.load == LoadOp::Load || a.stencilLoad == LoadOp::Load) &&
                a.initialLayout == ImageLayout::Undefined)
                throw std::invalid_argument("Cannot load undefined attachment contents");
        }
        for (const auto& s : subpasses) {
            if (!s.resolves.empty() && s.resolves.size() != s.colors.size())
                throw std::invalid_argument("Resolve attachment count must match color output count");
            auto check = [&](AttachmentReference ref, bool depth) {
                if (ref.attachment == UnusedAttachment)
                    return;
                if (ref.attachment >= attachments.size())
                    throw std::out_of_range("Attachment reference");
                const bool isDepth = _impl::helper::imageAspects(
                                         static_cast<vk::Format>(attachments[ref.attachment].format.type())) !=
                                     vk::ImageAspectFlagBits::eColor;
                if (isDepth != depth)
                    throw std::invalid_argument("Attachment role does not match format");
                const bool validLayout = depth ? ref.layout == ImageLayout::General ||
                                                     ref.layout == ImageLayout::DepthStencilAttachmentOptimal ||
                                                     ref.layout == ImageLayout::DepthStencilReadOnlyOptimal
                                               : ref.layout == ImageLayout::General ||
                                                     ref.layout == ImageLayout::ColorAttachmentOptimal;
                if (!validLayout)
                    throw std::invalid_argument("Invalid layout for attachment role");
            };
            std::optional<SampleCount> rasterSamples;
            for (auto ref : s.colors) {
                check(ref, false);
                if (ref.attachment == UnusedAttachment)
                    continue;
                auto samples = attachments[ref.attachment].samples;
                if (rasterSamples && *rasterSamples != samples)
                    throw std::invalid_argument("Color sample counts differ");
                rasterSamples = samples;
            }
            if (s.depthStencil && s.depthStencil->attachment != UnusedAttachment) {
                check(*s.depthStencil, true);
                if (rasterSamples && *rasterSamples != attachments[s.depthStencil->attachment].samples)
                    throw std::invalid_argument("Depth/color sample counts differ");
            }
            for (auto ref : s.inputs) {
                if (ref.attachment == UnusedAttachment)
                    continue;
                if (ref.attachment >= attachments.size())
                    throw std::out_of_range("Input attachment reference");
                if (ref.layout != ImageLayout::General && ref.layout != ImageLayout::ShaderReadOnlyOptimal &&
                    ref.layout != ImageLayout::DepthStencilReadOnlyOptimal)
                    throw std::invalid_argument("Invalid input attachment layout");
            }
            for (std::size_t i = 0; i < s.resolves.size(); ++i) {
                auto ref = s.resolves[i];
                check(ref, false);
                if (ref.attachment == UnusedAttachment)
                    continue;
                const auto src = s.colors[i].attachment;
                if (src == UnusedAttachment || attachments[src].samples == SampleCount::One ||
                    attachments[ref.attachment].samples != SampleCount::One ||
                    attachments[src].format.type() != attachments[ref.attachment].format.type())
                    throw std::invalid_argument("Invalid resolve attachment");
            }
            for (auto id : s.preserve) {
                if (id >= attachments.size())
                    throw std::out_of_range("Preserve attachment reference");
                auto used = [id](auto ref) { return ref.attachment == id; };
                if (std::ranges::any_of(s.colors, used) || std::ranges::any_of(s.inputs, used) ||
                    std::ranges::any_of(s.resolves, used) || (s.depthStencil && used(*s.depthStencil)))
                    throw std::invalid_argument("Cannot preserve an attachment used by the same subpass");
            }
        }
        for (const auto& d : dependencies) {
            const auto nonGraphics = PipelineStage::ComputeShader | PipelineStage::Transfer | PipelineStage::Host | PipelineStage::AllCommands;
            if ((d.source != ExternalSubpass && static_cast<std::uint32_t>(d.sourceStage & nonGraphics)) ||
                (d.destination != ExternalSubpass && static_cast<std::uint32_t>(d.destinationStage & nonGraphics)))
                throw std::invalid_argument("Internal subpass dependencies require graphics stages");
            if ((d.source != ExternalSubpass && d.source >= subpasses.size()) ||
                (d.destination != ExternalSubpass && d.destination >= subpasses.size()) ||
                (d.source == ExternalSubpass && d.destination == ExternalSubpass) ||
                (d.source != ExternalSubpass && d.destination != ExternalSubpass && d.source > d.destination) ||
                !static_cast<std::uint32_t>(d.sourceStage) || !static_cast<std::uint32_t>(d.destinationStage))
                throw std::invalid_argument("Invalid subpass dependency");
        }
    }
    const RenderPassDescription& RenderPassAccessor::description() const {
        if (!_data || !_data->description)
            throw std::logic_error("RenderPass has no description");
        return *_data->description;
    }
    RenderPass::RenderPass(LogicalDeviceAccessor device, RenderPassDescription desc) {
        desc.validate();
        if (!device._data)
            throw std::invalid_argument("RenderPass requires a device");
        auto data = std::make_shared<_impl::RenderPass_impl>();
        data->device = device._data;
        data->description = std::make_shared<const RenderPassDescription>(std::move(desc));
        std::vector<vk::AttachmentDescription> attachments;
        for (const auto& a : data->description->attachments)
            attachments.push_back({.format = static_cast<vk::Format>(a.format.type()),
                                   .samples = static_cast<vk::SampleCountFlagBits>(a.samples),
                                   .loadOp = static_cast<vk::AttachmentLoadOp>(a.load),
                                   .storeOp = static_cast<vk::AttachmentStoreOp>(a.store),
                                   .stencilLoadOp = static_cast<vk::AttachmentLoadOp>(a.stencilLoad),
                                   .stencilStoreOp = static_cast<vk::AttachmentStoreOp>(a.stencilStore),
                                   .initialLayout = static_cast<vk::ImageLayout>(a.initialLayout),
                                   .finalLayout = static_cast<vk::ImageLayout>(a.finalLayout)});
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
            if (s.depthStencil)
                r.depth = {.attachment = s.depthStencil->attachment,
                           .layout = static_cast<vk::ImageLayout>(s.depthStencil->layout)};
            subpasses.push_back({.pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
                                 .inputAttachmentCount = static_cast<std::uint32_t>(r.inputs.size()),
                                 .pInputAttachments = r.inputs.data(),
                                 .colorAttachmentCount = static_cast<std::uint32_t>(r.colors.size()),
                                 .pColorAttachments = r.colors.data(),
                                 .pResolveAttachments = r.resolves.empty() ? nullptr : r.resolves.data(),
                                 .pDepthStencilAttachment = s.depthStencil ? &r.depth : nullptr,
                                 .preserveAttachmentCount = static_cast<std::uint32_t>(s.preserve.size()),
                                 .pPreserveAttachments = s.preserve.data()});
        }
        std::vector<vk::SubpassDependency> dependencies;
        for (const auto& d : data->description->dependencies)
            dependencies.push_back(
                {.srcSubpass = d.source,
                 .dstSubpass = d.destination,
                 .srcStageMask = vk::PipelineStageFlags(static_cast<std::uint32_t>(d.sourceStage)),
                 .dstStageMask = vk::PipelineStageFlags(static_cast<std::uint32_t>(d.destinationStage)),
                 .srcAccessMask = vk::AccessFlags(static_cast<std::uint32_t>(d.sourceAccess)),
                 .dstAccessMask = vk::AccessFlags(static_cast<std::uint32_t>(d.destinationAccess)),
                 .dependencyFlags = d.byRegion ? vk::DependencyFlagBits::eByRegion : vk::DependencyFlags{}});
        vk::RenderPassCreateInfo info{.attachmentCount = static_cast<std::uint32_t>(attachments.size()),
                                      .pAttachments = attachments.data(),
                                      .subpassCount = static_cast<std::uint32_t>(subpasses.size()),
                                      .pSubpasses = subpasses.data(),
                                      .dependencyCount = static_cast<std::uint32_t>(dependencies.size()),
                                      .pDependencies = dependencies.data()};
        data->renderPass = data->device->device.createRenderPass(info);
        _data = std::move(data);
    }
    namespace {
        RenderPassDescription present_description(SwapchainAccessor swapchain, bool picking) {
            if (!swapchain._data)
                throw std::invalid_argument("Swapchain required");
            RenderPassDescription desc;
            desc.addColor({.format = swapchain.getFormat(),
                           .finalLayout = ImageLayout::Present});
            if (picking)
                desc.addColor({.format = data_format::UnsignedInt});
            return desc;
        }
    } // namespace
    RenderPass::RenderPass(SwapchainAccessor swapchain)
        : RenderPass(LogicalDeviceAccessor{swapchain._data ? swapchain._data->device : nullptr},
                     present_description(swapchain, false)) {}
    IDPickingRenderPass::IDPickingRenderPass(SwapchainAccessor swapchain) {
        RenderPass pass(LogicalDeviceAccessor{swapchain._data ? swapchain._data->device : nullptr},
                        present_description(swapchain, true));
        _data = pass._data;
        picking_buffer.resize(swapchain.images.size());
        recreate_buffer(swapchain._data->extent.width, swapchain._data->extent.height);
    }
    void IDPickingRenderPass::recreate_buffer(std::uint32_t width, std::uint32_t height) {
        for (auto& image : picking_buffer)
            image = Image<ImageType::ColorAttachment | ImageType::TransferSrc>(
                LogicalDeviceAccessor{_data->device}, width, height, data_format::UnsignedInt);
    }
    std::uint32_t IDPickingRenderPass::read(CommandManager manager, FenceHolder inFlight, std::uint32_t x,
                                            std::uint32_t y) {
        if (!inFlight.wait())
            throw std::runtime_error("Picking render did not complete");
        auto image = picking_buffer.at(manager.getCurrentBufferIdx());
        (void)command::ImageReadback::describe(image, ImageLayout::ColorAttachmentOptimal, {x, y, 1, 1});
        StagingBuffer<MemoryType::TransferDestination> destination(LogicalDeviceAccessor{_data->device}, sizeof(std::uint32_t));
        auto buffer = manager.makeExclusiveBuffer(1);
        auto native = buffer._data->commandBuffers.front();
        auto device = _data->device->device;
        auto fence = device.createFenceUnique({});
        native.begin(vk::CommandBufferBeginInfo{});
        command::ImageReadback(&destination, image, ImageLayout::ColorAttachmentOptimal, {x, y, 1, 1})
            .invoke(manager.getPool(), buffer, manager);
        native.end();
        manager._data->graphicsQueue.submit(vk::SubmitInfo{.commandBufferCount = 1, .pCommandBuffers = &native}, *fence);
        if (device.waitForFences(*fence, true, UINT64_MAX) != vk::Result::eSuccess)
            throw std::runtime_error("Picking transfer did not complete");
        auto mapped = destination.data(sizeof(std::uint32_t));
        std::uint32_t id;
        std::memcpy(&id, mapped.data, sizeof(id));
        return id;
    }
    _impl::RenderPass_impl::~RenderPass_impl() {
        if (device)
            device->device.destroyRenderPass(renderPass);
    }
} // namespace VSL_NAMESPACE
