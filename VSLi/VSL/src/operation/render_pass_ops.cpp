#include <VSL/operation/render_pass_ops.hpp>
#include "../vulkan/image_format.hpp"
#include <algorithm>
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
} // namespace VSL_NAMESPACE

namespace VSL_NAMESPACE::operation {
    namespace {
        std::uint32_t count(size_t size);
        std::uint32_t count(size_t size) {
            if (size > UINT32_MAX)
                throw std::length_error("Render pass count exceeds uint32_t");
            return static_cast<std::uint32_t>(size);
        }
    } // namespace
    void initialize_render_pass(impl::RenderPassImpl& target, impl::LogicalDeviceImpl& device, RenderPassDescription description) {
        description.validate();
        if (!device.device || !device.parent_device || !device.parent_device->device)
            throw std::invalid_argument("Render pass requires a live logical and physical device");
        (void)count(description.attachments.size());
        (void)count(description.subpasses.size());
        (void)count(description.dependencies.size());
        const auto limit = device.parent_device->device.getProperties().limits.maxColorAttachments;
        for (const auto& subpass : description.subpasses) {
            (void)count(subpass.inputs.size());
            (void)count(subpass.resolves.size());
            (void)count(subpass.preserve.size());
            if (subpass.colors.size() > limit)
                throw std::out_of_range("Render pass color output count exceeds the device limit");
        }
        target.device = &device;
        target.native_device = device.device;
        target.description.emplace(std::move(description));
        std::vector<vk::AttachmentDescription> attachments;
        for (const auto& a : target.description->attachments)
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
        std::vector<References> refs(target.description->subpasses.size());
        std::vector<vk::SubpassDescription> subpasses;
        for (std::size_t i = 0; i < refs.size(); ++i) {
            const auto& s = target.description->subpasses[i];
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
                                 .inputAttachmentCount = count(r.inputs.size()),
                                 .pInputAttachments = r.inputs.data(),
                                 .colorAttachmentCount = count(r.colors.size()),
                                 .pColorAttachments = r.colors.data(),
                                 .pResolveAttachments = r.resolves.empty() ? nullptr : r.resolves.data(),
                                 .pDepthStencilAttachment = s.depth_stencil ? &r.depth : nullptr,
                                 .preserveAttachmentCount = count(s.preserve.size()),
                                 .pPreserveAttachments = s.preserve.data()});
        }
        std::vector<vk::SubpassDependency> dependencies;
        for (const auto& d : target.description->dependencies)
            dependencies.push_back(
                {.srcSubpass = d.source,
                 .dstSubpass = d.destination,
                 .srcStageMask = vk::PipelineStageFlags(static_cast<std::uint32_t>(d.source_stage)),
                 .dstStageMask = vk::PipelineStageFlags(static_cast<std::uint32_t>(d.destination_stage)),
                 .srcAccessMask = vk::AccessFlags(static_cast<std::uint32_t>(d.source_access)),
                 .dstAccessMask = vk::AccessFlags(static_cast<std::uint32_t>(d.destination_access)),
                 .dependencyFlags = d.by_region ? vk::DependencyFlagBits::eByRegion : vk::DependencyFlags{}});
        vk::RenderPassCreateInfo info{.attachmentCount = count(attachments.size()),
                                      .pAttachments = attachments.data(),
                                      .subpassCount = count(subpasses.size()),
                                      .pSubpasses = subpasses.data(),
                                      .dependencyCount = count(dependencies.size()),
                                      .pDependencies = dependencies.data()};
        target.render_pass = device.device.createRenderPass(info);
    }
    void destroy_render_pass(impl::RenderPassImpl& target) noexcept {
        if (target.render_pass)
            target.native_device.destroyRenderPass(target.render_pass);
        target.render_pass = nullptr;
    }
} // namespace VSL_NAMESPACE::operation
