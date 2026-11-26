#pragma once
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/commands/image_readback.hpp>
#include <cstring>

namespace vsl_test {
    // テスト用の同期読み戻し。製品側のリソース再利用にはAHO::ImageReadbackを使う。
    inline std::vector<std::byte> read_image(vsl::CommandManager manager, vsl::ImageAccessor image,
                                             vsl::ImageLayout layout, vsl::ImageRegion region = {}) {
        const auto info = vsl::command::ImageReadback::Describe(image, layout, region);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> destination(
            vsl::LogicalDeviceAccessor{manager.data_->device}, info.byte_size);
        auto buffer = manager.make_exclusive_buffer(1);
        auto command = buffer.data_->command_buffers.front();
        auto device = manager.data_->device->device;
        auto fence = device.createFenceUnique({});
        command.begin(vk::CommandBufferBeginInfo{});
        vsl::command::ImageReadback(&destination, image, layout, info.region)
            .invoke(manager.get_pool(), buffer, manager);
        command.end();
        manager.data_->graphics_queue.submit(
            vk::SubmitInfo{.commandBufferCount = 1, .pCommandBuffers = &command}, *fence);
        if (device.waitForFences(*fence, true, UINT64_MAX) != vk::Result::eSuccess)
            throw std::runtime_error("Test image transfer did not complete");
        auto mapped = destination.data(info.byte_size);
        std::vector<std::byte> result(info.byte_size);
        std::memcpy(result.data(), mapped.data, result.size());
        return result;
    }
} // namespace vsl_test
