#pragma once
#include "../define.hpp"
#include "pv.hpp"

#include <memory>
#include <vector>
#include <array>

#include "command.hpp"
#include "image.hpp"
#include "swapchain.hpp"

namespace VSL_NAMESPACE {
    enum class ReadableAttachmentRole {
        Generic,
        ObjectId,
    };

    struct ReadableAttachmentDefinition {
        data_format::___Format format = data_format::UnsignedInt;
        ReadableAttachmentRole role = ReadableAttachmentRole::Generic;
        std::array<std::uint32_t, 4> clear = {0, 0, 0, 0};
    };

    struct RenderPassAccessor {
        std::shared_ptr<VSL_NAMESPACE::_impl::RenderPass_impl> _data;
    };

	struct RenderPass : public RenderPassAccessor {
		RenderPass(SwapchainAccessor swapchain);
	};

    struct ReadableRenderPass : public RenderPassAccessor {
        std::vector<ReadableAttachmentDefinition> readable_attachments;
        std::vector<std::vector<ImageAccessor>> readable_buffers;

        ReadableRenderPass(SwapchainAccessor swapchain, std::vector<ReadableAttachmentDefinition> attachments);

        void recreate_buffer(std::uint32_t width, std::uint32_t height);

        std::uint32_t read(std::size_t attachment,
                           vsl::CommandManager manager,
                           FenceHolder inFlight,
                           std::uint32_t x,
                           std::uint32_t y);

        ImageAccessor readable_image(std::size_t attachment, std::size_t frame);
    };
}
