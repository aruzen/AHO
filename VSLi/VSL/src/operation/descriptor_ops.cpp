#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/operation/descriptor_ops.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE::operation {
    void write_descriptor(vk::Device device, const vk::WriteDescriptorSet& write) {
        device.updateDescriptorSets(write, {});
    }
    void validate_descriptor_buffer(const impl::BufferImpl& buffer, vk::DescriptorType type, size_t offset, size_t bytes) {
        const bool uniform = type == vk::DescriptorType::eUniformBuffer || type == vk::DescriptorType::eUniformBufferDynamic;
        const bool storage = type == vk::DescriptorType::eStorageBuffer || type == vk::DescriptorType::eStorageBufferDynamic;
        if (!buffer.buffer || !buffer.use || (!uniform && !storage) ||
            !contain(buffer.usage, uniform ? MemoryType::UNIFORM_BUFFER : MemoryType::STORAGE_BUFFER))
            throw std::invalid_argument("Descriptor buffer usage/type mismatch");
        if (!bytes || offset >= buffer.requirement_size || (bytes != VK_WHOLE_SIZE && bytes > buffer.requirement_size - offset))
            throw std::out_of_range("Descriptor buffer range mismatch");
        const auto limits = buffer.device->parent_device->device.getProperties().limits;
        const auto limit = uniform ? limits.maxUniformBufferRange : limits.maxStorageBufferRange;
        const auto effective_range = bytes == VK_WHOLE_SIZE ? buffer.requirement_size - offset : bytes;
        if (effective_range > limit)
            throw std::out_of_range("Descriptor buffer exceeds the device range limit");
        const auto alignment = uniform ? limits.minUniformBufferOffsetAlignment : limits.minStorageBufferOffsetAlignment;
        if (alignment && offset % alignment)
            throw std::invalid_argument("Descriptor buffer offset violates device alignment");
    }
    void validate_descriptor_image(const impl::ImageImpl& image, vk::DescriptorType type) {
        auto usage = ImageType::SAMPLED;
        if (type == vk::DescriptorType::eStorageImage)
            usage = ImageType::STORAGE;
        else if (type == vk::DescriptorType::eInputAttachment)
            usage = ImageType::INPUT_ATTACHMENT;
        else if (type != vk::DescriptorType::eSampledImage && type != vk::DescriptorType::eCombinedImageSampler)
            throw std::invalid_argument("Descriptor image type mismatch");
        if (!image.view || !image.use || !contain(image.usage, usage))
            throw std::invalid_argument("Descriptor image usage mismatch");
    }
} // namespace VSL_NAMESPACE::operation
