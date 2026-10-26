#ifdef AHO_GPU_TESTS
#include <AHO/command/buffer_upload.hpp>
#include <VSL/vulkan/phase.hpp>
#include "support/vulkan_fixture.hpp"
#include "../support/buffer_customization.hpp"
#include <array>
#include <cstring>

namespace {
    using General = aho::resource::Buffer<aho::resource::BufferType::GENERAL>;
    using Host = aho::resource::Buffer<aho::resource::BufferType::GPU_FRIENDLY>;
    using Device = aho::resource::Buffer<aho::resource::BufferType::ON_GPU>;
    using aho_test::CustomCounters;
    using aho_test::NativeHost;
    using aho_test::CountingCustomization;

    class BufferUploadVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    bool BufferUploadVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }

    TEST_F(BufferUploadVulkanIntegrationTest, CPUUploadAndGPUCopiesShareTheSameCommandsAndRetainContents) {
        std::array<std::byte, 8> expected{std::byte{11}, std::byte{22}, std::byte{33}, std::byte{44}, std::byte{55}, std::byte{66}, std::byte{77}, std::byte{88}};
        General cpu;
        cpu.assign(expected);
        Device destination(device, 8);
        Host readback(device, 8);
        auto owner = destination.binding().owner();
        auto manager = make_manager();
        for (unsigned frame = 0; frame < 3; ++frame) {
            expected[0] = std::byte{static_cast<unsigned char>(11 + frame)};

            cpu.assign(expected);
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << destination.upload(cpu) << readback.upload(destination);
            EXPECT_TRUE(destination.busy());
            EXPECT_TRUE(readback.busy());
            EXPECT_FALSE(destination.wait(0));
            EXPECT_THROW(readback.write({}), std::logic_error);
            EXPECT_THROW(readback.read(0), std::logic_error);
            EXPECT_THROW(readback.map(), std::logic_error);
            EXPECT_THROW(phase << destination.upload(cpu), std::logic_error);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
            EXPECT_TRUE(destination.wait(0));
            EXPECT_EQ(destination.binding().owner(), owner);
            auto pixels = readback.read(8);
            EXPECT_EQ(std::memcmp(pixels.data(), expected.data(), 8), 0);
        }
    }

    TEST_F(BufferUploadVulkanIntegrationTest, FixedCustomBuffersAndOverlappingRangesUseCapabilityChecks) {
        auto custom = aho::resource::make_custom_buffer(NativeHost(device, 8));
        const std::array<std::byte, 4> expected{std::byte{17}, std::byte{29}, std::byte{43}, std::byte{71}};

        custom.assign(expected);
        EXPECT_EQ(custom.size(), 4u);
        EXPECT_EQ(custom.capacity(), 8u);
        EXPECT_EQ(custom.read(4)[2], expected[2]);
        std::array<std::byte, 9> too_large{};
        EXPECT_THROW(custom.assign(too_large), std::out_of_range);
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        Host target(device, 8);
        phase << target.upload(custom, 4);
        EXPECT_THROW(phase << target.upload(target, 1, 0, 4), std::invalid_argument);
        EXPECT_THROW(phase << target.upload(custom, 0, SIZE_MAX), std::out_of_range);
        EXPECT_THROW(phase << target.upload(custom, 6), std::out_of_range);
        EXPECT_THROW(phase << target.upload(custom, 0, 1, SIZE_MAX), std::out_of_range);
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        EXPECT_EQ(std::memcmp(target.read(4, 4).data(), expected.data(), 4), 0);
    }

    TEST_F(BufferUploadVulkanIntegrationTest, PendingTransfersRejectOtherPhasesAndCancellationAllowsReuse) {
        auto manager = make_manager();
        auto other = manager.independent();
        Host source(device, 4);
        Device destination(device, 4);
        const std::array<std::byte, 4> bytes{};

        source.write(bytes);
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << destination.upload(source);
            auto second = other.start_phase<vsl::ComputePhase>();
            EXPECT_THROW(second << destination.upload(source), std::logic_error);
            second.cancel();
            phase.cancel();
        }
        EXPECT_FALSE(source.busy());
        EXPECT_TRUE(source.wait(0));
        source.assign(bytes);
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << destination.upload(source);
        phase.submit();
        ASSERT_TRUE(phase.completion().wait());
    }

    TEST_F(BufferUploadVulkanIntegrationTest, CancelledCPUUploadReusesStagingWithTheNewPayload) {
        auto manager = make_manager();
        General cpu(4);
        Device destination(device, 4);
        Host result(device, 4);
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << destination.upload(cpu);
            phase.cancel();
        }
        const std::array<std::byte, 4> expected{std::byte{17}, std::byte{37}, std::byte{73}, std::byte{255}};

        cpu.assign(expected);
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << destination.upload(cpu) << result.upload(destination);
        phase.submit();
        ASSERT_TRUE(phase.completion().wait());
        EXPECT_EQ(std::memcmp(result.read(4).data(), expected.data(), 4), 0);
    }

    TEST_F(BufferUploadVulkanIntegrationTest, SubmissionOwnsCustomizationWithoutKeepingCompletionInACycle) {
        auto manager = make_manager();
        std::weak_ptr<CustomCounters> weak;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            {
                auto counters = std::make_shared<CustomCounters>();
                weak = counters;
                auto source = aho::resource::make_custom_buffer(NativeHost(device, 4), CountingCustomization{device, counters});
                source.write(std::array<std::byte, 4>{});
                Device destination(device, 4);
                phase << destination.upload(source);
            }
            EXPECT_FALSE(weak.expired());
            phase.submit();
            ASSERT_TRUE(phase.completion().wait());
        }
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase.submit();
            ASSERT_TRUE(phase.completion().wait());
        }
        EXPECT_TRUE(weak.expired());
    }

    TEST_F(BufferUploadVulkanIntegrationTest, EmptyUploadsAndInvalidRecordingDoNotSubmitOrClaimResources) {
        auto manager = make_manager();
        Host source(device);
        Device destination(device, 0);
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << destination.upload(source) << destination.upload(General{});
            EXPECT_FALSE(destination.busy());
            EXPECT_FALSE(source.busy());
            phase.cancel();
        }
        EXPECT_THROW(destination.native(), std::logic_error);
        auto command = destination.upload(source);
        EXPECT_THROW(command.invoke(manager.get_pool(), manager.get_buffer(), manager), std::logic_error);
    }
} // namespace
#endif
