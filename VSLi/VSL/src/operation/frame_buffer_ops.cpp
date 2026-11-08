#include <VSL/operation/frame_buffer_ops.hpp>
#include <algorithm>
#include "../vulkan/image_format.hpp"
#include <type_traits>
#include <stdexcept>

namespace VSL_NAMESPACE::operation {
    namespace {
        vk::Framebuffer create_native(void*, vk::Device device, const vk::FramebufferCreateInfo& info);
        void destroy_native(void*, vk::Device device, vk::Framebuffer framebuffer) noexcept;
        constexpr FrameBufferNativeFactory NATIVE_FACTORY{nullptr, create_native, destroy_native};
        void validate_image(const impl::ImageImpl* image, const impl::LogicalDeviceImpl& device,
                            const FrameBufferInput& input, AttachmentId attachment);
        vk::ClearValue convert_clear(const ClearValue& value);
        vk::Framebuffer create_native(void*, vk::Device device, const vk::FramebufferCreateInfo& info) {
            return device.createFramebuffer(info);
        }
        void destroy_native(void*, vk::Device device, vk::Framebuffer framebuffer) noexcept {
            device.destroyFramebuffer(framebuffer);
        }
        vk::ClearValue convert_clear(const ClearValue& value) {
            vk::ClearValue result{};
            std::visit([&](auto clear) {
                using T = decltype(clear);
                if constexpr (std::is_same_v<T, ClearColor>)
                    result.color = vk::ClearColorValue(std::array<float, 4>{clear.r, clear.g, clear.b, clear.a});
                else if constexpr (std::is_same_v<T, ClearInt>)
                    result.color = vk::ClearColorValue(std::array<std::int32_t, 4>{clear.r, clear.g, clear.b, clear.a});
                else if constexpr (std::is_same_v<T, ClearUint>)
                    result.color = vk::ClearColorValue(std::array<std::uint32_t, 4>{clear.r, clear.g, clear.b, clear.a});
                else
                    result.depthStencil = vk::ClearDepthStencilValue{.depth = clear.depth, .stencil = clear.stencil};
            },
                       value);
            return result;
        }
        void validate_image(const impl::ImageImpl* image, const impl::LogicalDeviceImpl& device,
                            const FrameBufferInput& input, AttachmentId attachment) {
            const auto& description = *input.pass->description;
            const auto& expected = description.attachments[attachment];
            if (!image || !image->image || !image->view || !image->use || image->device != &device ||
                image->format != static_cast<vk::Format>(expected.format.type()) || image->samples != expected.samples ||
                image->width < input.width || image->height < input.height || image->count < input.layers)
                throw std::invalid_argument("Framebuffer attachment device/format/sample/extent mismatch");
            const auto require = [&](ImageType usage) {
                if (!contain(image->usage, usage))
                    throw std::invalid_argument("Framebuffer image missing required usage");
            };
            for (const auto& subpass : description.subpasses) {
                for (const auto reference : subpass.colors)
                    if (reference.attachment == attachment)
                        require(ImageType::COLOR_ATTACHMENT);
                for (const auto reference : subpass.resolves)
                    if (reference.attachment == attachment)
                        require(ImageType::COLOR_ATTACHMENT);
                for (const auto reference : subpass.inputs)
                    if (reference.attachment == attachment)
                        require(ImageType::INPUT_ATTACHMENT);
                if (subpass.depth_stencil && subpass.depth_stencil->attachment == attachment)
                    require(ImageType::DEPTH_STENCIL_ATTACHMENT);
            }
        }
    } // namespace
    std::vector<ClearValue> default_frame_buffer_clears(const RenderPassDescription& description) {
        std::vector<ClearValue> values;
        values.reserve(description.attachments.size());
        for (const auto& attachment : description.attachments) {
            switch (impl::helper::clear_kind(static_cast<vk::Format>(attachment.format.type()))) {
                case 1:
                    values.emplace_back(ClearInt{});
                    break;
                case 2:
                    values.emplace_back(ClearUint{});
                    break;
                case 3:
                    values.emplace_back(ClearDepthStencil{});
                    break;
                default:
                    values.emplace_back(ClearColor{});
                    break;
            }
        }
        return values;
    }
    void validate_frame_buffer_clears(const RenderPassDescription& description, std::span<const ClearValue> values) {
        if (values.size() != description.attachments.size() || values.size() > UINT32_MAX)
            throw std::invalid_argument("Clear count must match attachments");
        for (size_t i = 0; i < values.size(); ++i) {
            const auto& attachment = description.attachments[i];
            if ((attachment.load == LoadOp::CLEAR || attachment.stencil_load == LoadOp::CLEAR) &&
                values[i].index() != impl::helper::clear_kind(static_cast<vk::Format>(attachment.format.type())))
                throw std::invalid_argument("Clear type does not match attachment format");
        }
    }
    vk::Rect2D frame_buffer_area(const impl::FrameBufferImpl& target, vk::Rect2D area) {
        if (!area.extent.width && !area.extent.height)
            area.extent = target.extent;
        if (area.offset.x < 0 || area.offset.y < 0 || !area.extent.width || !area.extent.height ||
            std::uint64_t(area.offset.x) + area.extent.width > target.extent.width ||
            std::uint64_t(area.offset.y) + area.extent.height > target.extent.height)
            throw std::out_of_range("Render area exceeds framebuffer");
        return area;
    }
    FrameBufferBeginScratch::FrameBufferBeginScratch(const impl::FrameBufferImpl& target, std::uint32_t index, vk::Rect2D requested, std::span<const ClearValue> values)
        : pass(target.render_pass ? target.render_pass->render_pass : nullptr), framebuffer(target.swap_chain_framebuffers.at(index)), area(frame_buffer_area(target, requested)) {
        if (!pass || !framebuffer || !target.render_pass->description)
            throw std::logic_error("Framebuffer has no native pass or handle");
        validate_frame_buffer_clears(*target.render_pass->description, values);
        clears.reserve(values.size());
        for (const auto& value : values)
            clears.push_back(convert_clear(value));
    }
    void record_frame_buffer_begin(vk::CommandBuffer buffer, const FrameBufferBeginScratch& scratch) {
        const vk::RenderPassBeginInfo info{.renderPass = scratch.pass, .framebuffer = scratch.framebuffer, .renderArea = scratch.area, .clearValueCount = static_cast<std::uint32_t>(scratch.clears.size()), .pClearValues = scratch.clears.data()};
        buffer.beginRenderPass(info, vk::SubpassContents::eInline);
    }
    void record_frame_buffer_end(vk::CommandBuffer buffer) {
        buffer.endRenderPass();
    }
    void record_next_subpass(vk::CommandBuffer buffer) {
        buffer.nextSubpass(vk::SubpassContents::eInline);
    }
    void initialize_frame_buffer(impl::FrameBufferImpl& target, impl::LogicalDeviceImpl& device, FrameBufferInput input) {
        initialize_frame_buffer(target, device, std::move(input), NATIVE_FACTORY);
    }
    void initialize_frame_buffer(impl::FrameBufferImpl& target, impl::LogicalDeviceImpl& device, FrameBufferInput input, const FrameBufferNativeFactory& factory) {
        if (std::ranges::any_of(target.swap_chain_framebuffers, [](const auto framebuffer) { return bool(framebuffer); }))
            throw std::logic_error("Framebuffer target is already initialized");
        if (!factory.create || !factory.destroy)
            throw std::invalid_argument("Framebuffer native factory requires create and destroy operations");
        if (!device.device || !device.parent_device || !device.parent_device->device || !input.pass ||
            !input.pass->render_pass || !input.pass->description || input.pass->device != &device ||
            !input.width || !input.height || !input.layers || input.attachments.empty())
            throw std::invalid_argument("Framebuffer requires a live pass/device, positive extent and at least one frame");
        const auto& description = *input.pass->description;
        if (description.attachments.size() > UINT32_MAX || input.attachments.size() > UINT32_MAX)
            throw std::length_error("Framebuffer attachment or frame count exceeds uint32_t");
        const auto limits = device.parent_device->device.getProperties().limits;
        if (input.width > limits.maxFramebufferWidth || input.height > limits.maxFramebufferHeight || input.layers > limits.maxFramebufferLayers)
            throw std::out_of_range("Framebuffer extent exceeds device limits");
        // 全入力とCPU確保を終えてからnative生成する。
        for (const auto& row : input.attachments) {
            if (row.size() != description.attachments.size())
                throw std::invalid_argument("Framebuffer attachment count mismatch");
            for (size_t i = 0; i < row.size(); ++i)
                validate_image(row[i], device, input, static_cast<AttachmentId>(i));
        }
        target.device = &device;
        target.native_device = device.device;
        target.render_pass = input.pass;
        target.extent = vk::Extent2D{input.width, input.height};
        target.layers = input.layers;
        target.attachments = std::move(input.attachments);
        target.swap_chain_framebuffers.resize(target.attachments.size());
        std::vector<vk::ImageView> views(description.attachments.size());
        const vk::FramebufferCreateInfo info{.renderPass = input.pass->render_pass,
                                             .attachmentCount = static_cast<std::uint32_t>(views.size()),
                                             .pAttachments = views.data(),
                                             .width = input.width,
                                             .height = input.height,
                                             .layers = input.layers};
        try {
            for (size_t i = 0; i < target.attachments.size(); ++i) {
                for (size_t j = 0; j < views.size(); ++j)
                    views[j] = target.attachments[i][j]->view;
                const auto framebuffer = factory.create(factory.context, device.device, info);
                if (!framebuffer)
                    throw std::runtime_error("Framebuffer native creation returned an empty handle");
                target.swap_chain_framebuffers[i] = framebuffer;
            }
        } catch (...) {
            for (size_t i = target.swap_chain_framebuffers.size(); i > 0; --i) {
                auto& framebuffer = target.swap_chain_framebuffers[i - 1];
                if (framebuffer)
                    factory.destroy(factory.context, device.device, framebuffer);
                framebuffer = nullptr;
            }
            throw;
        }
    }
    void destroy_frame_buffer(impl::FrameBufferImpl& target) noexcept {
        for (size_t i = target.swap_chain_framebuffers.size(); i > 0; --i) {
            auto& framebuffer = target.swap_chain_framebuffers[i - 1];
            if (framebuffer)
                target.native_device.destroyFramebuffer(framebuffer);
            framebuffer = nullptr;
        }
    }
} // namespace VSL_NAMESPACE::operation
