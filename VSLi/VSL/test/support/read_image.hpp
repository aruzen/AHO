#pragma once
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/commands/image_readback.hpp>
#include <cstring>

namespace vsl_test {
    // テスト用の同期読み戻し。製品側のリソース再利用にはAHO::ImageReadbackを使う。
    inline std::vector<std::byte> read_image(vsl::CommandManager manager, vsl::ImageAccessor image,
                                             vsl::ImageLayout layout, vsl::ImageRegion region = {}) {
        const auto info = vsl::command::ImageReadback::describe(image, layout, region);
        vsl::StagingBuffer<vsl::MemoryType::TransferDestination> destination(
            vsl::LogicalDeviceAccessor{manager._data->device}, info.byteSize);
        auto buffer = manager.makeExclusiveBuffer(1);
        auto command = buffer._data->commandBuffers.front();
        auto device = manager._data->device->device;
        auto fence = device.createFenceUnique({});
        command.begin(vk::CommandBufferBeginInfo{});
        vsl::command::ImageReadback(&destination, image, layout, info.region)
            .invoke(manager.getPool(), buffer, manager);
        command.end();
        manager._data->graphicsQueue.submit(
            vk::SubmitInfo{.commandBufferCount = 1, .pCommandBuffers = &command}, *fence);
        if (device.waitForFences(*fence, true, UINT64_MAX) != vk::Result::eSuccess)
            throw std::runtime_error("Test image transfer did not complete");
        auto mapped = destination.data(info.byteSize);
        std::vector<std::byte> result(info.byteSize);
        std::memcpy(result.data(), mapped.data, result.size());
        return result;
    }
} // namespace vsl_test
