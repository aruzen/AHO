#pragma once
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vulkan/commands/image_readback.hpp>
#include <VSL/vulkan/phase.hpp>
#include <cstring>

namespace vsl_test {
    // テスト用の同期読み戻し。製品側のリソース再利用にはAHO::ImageReadbackを使う。
    inline std::vector<std::byte> read_image(vsl::CommandManager manager, vsl::ImageAccessor image,
                                             vsl::ImageLayout layout, vsl::ImageRegion region = {}) {
        const auto info = vsl::command::ImageReadback::Describe(image, layout, region);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> destination(
            vsl::LogicalDeviceAccessor{manager.data_->device}, info.byte_size);
        auto transfer = manager.independent(1);
        auto phase = transfer.start_phase<vsl::ComputePhase>();
        phase << vsl::command::ImageReadback(&destination, image, layout, info.region);
        if (phase.submit() != vsl::ComputePhase::SubmitResult::SUCCESS || !phase.completion().wait())
            throw std::runtime_error("Test image transfer did not complete");
        std::vector<std::byte> result(info.byte_size);
        destination.read_into(result);
        return result;
    }
} // namespace vsl_test
