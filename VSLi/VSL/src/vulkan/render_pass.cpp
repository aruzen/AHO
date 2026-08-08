#include <VSL/vulkan/render_pass.hpp>

#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/image.hpp>
#include <VSL/vulkan/phase.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>
#include <VSL/vulkan/commands/copy_image_to_buffer.hpp>
#include <VSL/exceptions.hpp>

#include <source_location>

VSL_NAMESPACE::RenderPass::RenderPass(SwapchainAccessor swapchain)
{
    _data = std::shared_ptr<VSL_NAMESPACE::_impl::RenderPass_impl>(new VSL_NAMESPACE::_impl::RenderPass_impl);
    _data->device = swapchain._data->device;

    // TODO ここのAPIの提供ができてない
    vk::AttachmentDescription colorAttachment;
    colorAttachment.format = swapchain._data->swapChainImageFormat;
    colorAttachment.samples = vk::SampleCountFlagBits::e1;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference colorAttachmentRef;
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription subpass;
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    vk::SubpassDependency dependency;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    vk::RenderPassCreateInfo renderPassInfo;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    _data->renderPass = _data->device->device.createRenderPass(renderPassInfo);
}

VSL_NAMESPACE::ReadableRenderPass::ReadableRenderPass(
        SwapchainAccessor swapchain,
        std::vector<ReadableAttachmentDefinition> attachments)
{
    if (swapchain._data == nullptr || swapchain._data->device == nullptr) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "ReadableRenderPass",
                "ReadableRenderPass creation requires a valid swapchain.",
                std::source_location::current());
    }
    if (attachments.empty()) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "ReadableRenderPass",
                "ReadableRenderPass requires at least one readable attachment.",
                std::source_location::current());
    }

    _data = std::shared_ptr<VSL_NAMESPACE::_impl::RenderPass_impl>(new VSL_NAMESPACE::_impl::RenderPass_impl);
    _data->device = swapchain._data->device;
    readable_attachments = std::move(attachments);

    std::vector<vk::AttachmentDescription> renderAttachments;
    renderAttachments.push_back(vk::AttachmentDescription{
            .format = swapchain._data->swapChainImageFormat,
            .samples = vk::SampleCountFlagBits::e1,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::ePresentSrcKHR,
    });

    std::vector<vk::AttachmentReference> colorAttachmentRefs;
    colorAttachmentRefs.push_back(vk::AttachmentReference{
            .attachment = 0,
            .layout = vk::ImageLayout::eColorAttachmentOptimal,
    });

    for (std::uint32_t i = 0; i < readable_attachments.size(); i++) {
        renderAttachments.push_back(vk::AttachmentDescription{
                .format = static_cast<vk::Format>(readable_attachments[i].format.type()),
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout = vk::ImageLayout::eUndefined,
                .finalLayout = vk::ImageLayout::eColorAttachmentOptimal,
        });
        colorAttachmentRefs.push_back(vk::AttachmentReference{
                .attachment = i + 1,
                .layout = vk::ImageLayout::eColorAttachmentOptimal,
        });
    }

    vk::SubpassDescription subpass;
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = colorAttachmentRefs.size();
    subpass.pColorAttachments = colorAttachmentRefs.data();

    vk::SubpassDependency dependency;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    vk::RenderPassCreateInfo renderPassInfo;
    renderPassInfo.attachmentCount = renderAttachments.size();
    renderPassInfo.pAttachments = renderAttachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    _data->renderPass = _data->device->device.createRenderPass(renderPassInfo);

    readable_buffers.resize(readable_attachments.size());
    readable_frame_ready = std::make_shared<std::vector<bool>>(swapchain.getSwapImageSize(), false);
    for (size_t attachment = 0; attachment < readable_attachments.size(); attachment++) {
        auto &buffers = readable_buffers[attachment];
        buffers.reserve(swapchain.getSwapImageSize());
        for (size_t i = 0; i < swapchain.getSwapImageSize(); i++) {
            Image<ImageType::ColorAttachment | ImageType::TransferSrc> image(
                    LogicalDeviceAccessor{_data->device},
                    swapchain._data->extent.width,
                    swapchain._data->extent.height,
                    readable_attachments[attachment].format);
            buffers.push_back(image);
        }
    }
}

void vsl::ReadableRenderPass::recreate_buffer(std::uint32_t width, std::uint32_t height)
{
    if (_data == nullptr || _data->device == nullptr) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "ReadableRenderPass",
                "ReadableRenderPass recreate requires a valid device.",
                std::source_location::current());
    }
    for (size_t attachment = 0; attachment < readable_buffers.size(); attachment++) {
        auto &buffers = readable_buffers[attachment];
        for (size_t i = 0; i < buffers.size(); i++) {
            Image<ImageType::ColorAttachment | ImageType::TransferSrc> image(
                    LogicalDeviceAccessor{_data->device},
                    width,
                    height,
                    readable_attachments[attachment].format);
            buffers[i] = image;
        }
    }
    if (readable_frame_ready != nullptr)
        std::fill(readable_frame_ready->begin(), readable_frame_ready->end(), false);
}

std::uint32_t vsl::ReadableRenderPass::read(std::size_t attachment,
                                            vsl::CommandManager manager,
                                            FenceHolder inFlight,
                                            std::uint32_t x,
                                            std::uint32_t y)
{
    if (attachment >= readable_buffers.size()) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "ReadableRenderPass",
                "Readable attachment index is out of range.",
                std::source_location::current());
    }

    const auto frameCount = readable_buffers[attachment].size();
    if (frameCount == 0) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "ReadableRenderPass",
                "Readable attachment has no frame buffers.",
                std::source_location::current());
    }

    const auto current = manager.getCurrentBufferIdx();
    if (current >= frameCount) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "ReadableRenderPass",
                "Readable frame index is out of range.",
                std::source_location::current());
    }

    const auto readFrame = (current + frameCount - 1) % frameCount;
    if (readable_frame_ready == nullptr ||
        readFrame >= readable_frame_ready->size() ||
        !(*readable_frame_ready)[readFrame]) {
        return 0;
    }

    auto &image = readable_buffers[attachment][readFrame];
    if (x >= image._data->width || y >= image._data->height) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "ReadableRenderPass",
                "Readback pixel is out of range.",
                std::source_location::current());
    }

    std::uint32_t resultValue = 0;
    StagingBuffer<vsl::MemoryType::TransferDestination> staging_buffer(
            LogicalDeviceAccessor{_data->device},
            sizeof(std::uint32_t),
            manager);
    {
        auto phase = manager.startPhase<ComputePhase>(std::nullopt, std::nullopt, inFlight);

        phase << command::ChangeImageBarrier(image,
                                             vsl::ImageLayout::ColorAttachmentOptimal,
                                             vsl::ImageLayout::TransferSrcOptimal);

        vk::BufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = vk::Offset3D{int32_t(x), int32_t(y), 0};
        region.imageExtent = vk::Extent3D{1, 1, 1};

        manager.getBuffer()._data->commandBuffers[current].copyImageToBuffer(
                image._data->image,
                vk::ImageLayout::eTransferSrcOptimal,
                staging_buffer._data->buffer,
                region);

        phase << command::ChangeImageBarrier(image,
                                             vsl::ImageLayout::TransferSrcOptimal,
                                             vsl::ImageLayout::ColorAttachmentOptimal);
    }
    inFlight.wait();
    {
        auto data = staging_buffer.data();
        std::memcpy(&resultValue, data.data, sizeof(std::uint32_t));
    }
    return resultValue;
}

vsl::ImageAccessor vsl::ReadableRenderPass::readable_image(std::size_t attachment, std::size_t frame)
{
    if (attachment >= readable_buffers.size() || frame >= readable_buffers[attachment].size()) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "ReadableRenderPass",
                "Readable image index is out of range.",
                std::source_location::current());
    }
    return readable_buffers[attachment][frame];
}

void vsl::ReadableRenderPass::mark_frame_ready(std::size_t frame)
{
    if (readable_frame_ready == nullptr || frame >= readable_frame_ready->size())
        return;
    (*readable_frame_ready)[frame] = true;
}

VSL_NAMESPACE::_impl::RenderPass_impl::~RenderPass_impl()
{
    if (device != nullptr && renderPass)
        device->device.destroyRenderPass(renderPass);
}
