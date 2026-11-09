#include <VSL/operation/image_ops.hpp>
#include <VSL/exceptions.hpp>
#include "../vulkan/image_format.hpp"
#include <stdexcept>

namespace VSL_NAMESPACE::operation {
    namespace {
        constexpr VkImageUsageFlags IMAGE_VIEW_USAGES = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                                                        VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR |
                                                        VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT | VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR |
                                                        VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR | VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR | VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR |
                                                        VK_IMAGE_USAGE_SAMPLE_WEIGHT_BIT_QCOM | VK_IMAGE_USAGE_SAMPLE_BLOCK_MATCH_BIT_QCOM
#if defined(VK_KHR_video_encode_quantization_map)
                                                        | VK_IMAGE_USAGE_VIDEO_ENCODE_QUANTIZATION_DELTA_MAP_BIT_KHR | VK_IMAGE_USAGE_VIDEO_ENCODE_EMPHASIS_MAP_BIT_KHR
#endif
            ;
        std::uint32_t select_image_memory(impl::LogicalDeviceImpl& device, std::uint32_t types, vk::MemoryPropertyFlags required) {
            const auto properties = device.parent_device->device.getMemoryProperties();
            for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i)
                if ((types & (1u << i)) && (properties.memoryTypes[i].propertyFlags & required) == required)
                    return i;
            throw exceptions::RuntimeException("MemoryTypeNotFound", "No compatible memory type satisfies the requested properties");
        }
        void describe_image(impl::ImageImpl& target, impl::LogicalDeviceImpl& device, const ImageCreateInfo& info) {
            if (!device.device || !info.width || !info.height || !info.layers || info.usage == ImageType::NONE ||
                info.format.type() == data_format::FormatTypes::UNDEFINED)
                throw std::invalid_argument("Invalid image device/extent/format/usage");
            const auto samples = static_cast<std::uint32_t>(info.samples);
            if (!samples || samples > 64 || (samples & (samples - 1)))
                throw std::invalid_argument("Invalid image sample count");
            target.device = &device;
            target.native_device = device.device;
            target.width = info.width;
            target.height = info.height;
            target.count = info.layers;
            target.usage = info.usage;
            target.samples = info.samples;
            target.format = static_cast<vk::Format>(info.format.type());
        }
        void create_image_view(impl::ImageImpl& target, const ImageViewNativeCalls& calls = {}) {
            // 転送専用imageにはviewが不要で、view作成を許すusageもない。
            if ((static_cast<VkImageUsageFlags>(target.usage) & IMAGE_VIEW_USAGES) == 0)
                return;
            const vk::ImageViewCreateInfo info{
                .image = target.image,
                .viewType = target.count == 1 ? vk::ImageViewType::e2D : vk::ImageViewType::e2DArray,
                .format = target.format,
                .subresourceRange = {impl::helper::image_aspects(target.format), 0, 1, 0, target.count},
            };
            VkImageView view = VK_NULL_HANDLE;
            const auto result = calls.create ? calls.create(calls.context, target.native_device, &static_cast<const VkImageViewCreateInfo&>(info), &view)
                                             : vkCreateImageView(target.native_device, &static_cast<const VkImageViewCreateInfo&>(info), nullptr, &view);
            if (result != VK_SUCCESS)
                vk::detail::throwResultException(static_cast<vk::Result>(result), "Image view creation failed");
            if (!view)
                throw std::runtime_error("Image view creation returned an empty handle");
            target.view = view;
        }
    } // namespace

    void initialize_image(impl::ImageImpl& target, impl::LogicalDeviceImpl& device, const ImageCreateInfo& info) {
        try {
            describe_image(target, device, info);
            if (info.initial_layout != ImageLayout::UNDEFINED && info.initial_layout != ImageLayout::PREINITIALIZED)
                throw std::invalid_argument("Image creation requires Undefined or Preinitialized layout");
            target.owns_image = true;
            const vk::ImageCreateInfo native{
                .imageType = vk::ImageType::e2D,
                .format = target.format,
                .extent = {info.width, info.height, 1},
                .mipLevels = 1,
                .arrayLayers = info.layers,
                .samples = static_cast<vk::SampleCountFlagBits>(info.samples),
                .tiling = vk::ImageTiling::eOptimal,
                .usage = vk::ImageUsageFlags(static_cast<unsigned int>(info.usage)),
                .initialLayout = static_cast<vk::ImageLayout>(info.initial_layout),
            };
            target.image = target.native_device.createImage(native);
            const auto requirements = target.native_device.getImageMemoryRequirements(target.image);
            const auto memory_type = select_image_memory(device, requirements.memoryTypeBits,
                                                         vk::MemoryPropertyFlags(static_cast<unsigned int>(info.properties)));
            target.memory = target.native_device.allocateMemory({.allocationSize = requirements.size, .memoryTypeIndex = memory_type});
            target.native_device.bindImageMemory(target.image, target.memory, 0);
            create_image_view(target);
        } catch (...) {
            destroy_image(target);
            throw;
        }
    }
    void initialize_borrowed_image(impl::ImageImpl& target, impl::LogicalDeviceImpl& device,
                                   const ImageCreateInfo& info, vk::Image image, const ImageViewNativeCalls& calls) {
        try {
            describe_image(target, device, info);
            if (!image)
                throw std::invalid_argument("Borrowed image requires a native image");
            target.image = image;
            create_image_view(target, calls);
        } catch (...) {
            destroy_image(target);
            throw;
        }
    }
    void destroy_image(impl::ImageImpl& target) noexcept {
        if (!target.native_device)
            return;
        target.native_device.destroyImageView(target.view);
        if (target.owns_image) {
            target.native_device.destroyImage(target.image);
            target.native_device.freeMemory(target.memory);
        }
        target.view = nullptr;
        target.image = nullptr;
        target.memory = nullptr;
        target.owns_image = false;
    }
} // namespace VSL_NAMESPACE::operation
