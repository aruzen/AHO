#include "support/vulkan_fixture.hpp"
#include <array>
#include <limits>
#include "support/compile_probe.hpp"
#include "support/render_fixture.hpp"
#include <VSL/vulkan/buffer.hpp>
#include <VSL/vulkan/commands/image_readback.hpp>

namespace {
    class BufferVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    using HostBuffer = vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION>;

    enum ElementCount : short {
        NO_ELEMENTS = 0,
        THREE_ELEMENTS = 3,
        NEGATIVE_ELEMENTS = -1
    };

    class BufferSharingVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    using ConcurrentHostBuffer = vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION, vsl::MemoryProperty::NONE, vsl::SharingMode::CONCURRENT>;

    struct FlushTrackingBuffer : vsl::Buffer<vsl::MemoryType::TRANSFER_SOURCE, vsl::MemoryProperty::HOST_VISIBLE> {
        using Base = vsl::Buffer<vsl::MemoryType::TRANSFER_SOURCE, vsl::MemoryProperty::HOST_VISIBLE>;
        using Base::Base;
        int flush_calls = 0;
        void flush(LocalBufferHolder& holder) override;
    };

    using ReadbackBuffer = vsl::Buffer<vsl::MemoryType::TRANSFER_DESTINATION, vsl::MemoryProperty::HOST_VISIBLE>;

    class PersistentBufferVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    bool BufferVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }
    TEST_F(BufferVulkanIntegrationTest, MapsCopiesAndReadsBackSingleValues) {
        if (!host_coherent_available())
            GTEST_SKIP() << "HostVisible|HostCoherent memory unavailable";
        HostBuffer buffer(device, std::size_t{64});
        EXPECT_GE(buffer.size(), 64u);
        const std::uint32_t expected = 0x10203040;
        EXPECT_TRUE(buffer.copy(expected));
        auto mapped = buffer.data(sizeof(expected));
        std::uint32_t actual = 0;
        std::memcpy(&actual, mapped.data, sizeof(actual));
        EXPECT_EQ(actual, expected);
        EXPECT_EQ(mapped.parent, &buffer);
        EXPECT_EQ(mapped.size, sizeof(expected));
        EXPECT_EQ(mapped.offset, 0u);
        mapped.flush();
    }
    TEST_F(BufferVulkanIntegrationTest, MappedPointerCopiesPreserveOverlappingSourceRanges) {
        if (!host_coherent_available())
            GTEST_SKIP() << "HostVisible|HostCoherent memory unavailable";
        const std::array<std::uint32_t, 8> initial{1, 2, 3, 4, 5, 6, 7, 8};
        HostBuffer buffer(device, initial);
        auto whole = buffer.data();
        auto tail = buffer.data(6 * sizeof(std::uint32_t), sizeof(std::uint32_t));
        EXPECT_TRUE(tail.copy(static_cast<std::uint32_t*>(whole.data), std::size_t{6}));
        std::array<std::uint32_t, 8> actual{};
        std::memcpy(actual.data(), whole.data, sizeof(actual));
        EXPECT_EQ(actual, (std::array<std::uint32_t, 8>{1, 1, 2, 3, 4, 5, 6, 8}));
        ASSERT_TRUE(buffer.copy(initial));
        auto head = buffer.data(6 * sizeof(std::uint32_t));
        EXPECT_TRUE(head.copy(static_cast<std::uint32_t*>(whole.data) + 1, std::size_t{6}));
        std::memcpy(actual.data(), whole.data, sizeof(actual));
        EXPECT_EQ(actual, (std::array<std::uint32_t, 8>{2, 3, 4, 5, 6, 7, 7, 8}));
    }

    TEST_F(BufferVulkanIntegrationTest, PointerConstructorAndPointerCopyPreserveArrayContents) {
        if (!host_coherent_available())
            GTEST_SKIP() << "HostVisible|HostCoherent memory unavailable";
        std::array<std::uint32_t, 4> values{3, 5, 7, 11};
        HostBuffer buffer(device, values.data(), values.size());
        {
            auto mapped = buffer.data(sizeof(values));
            EXPECT_EQ(std::memcmp(mapped.data, values.data(), sizeof(values)), 0);
        }
        values = {13, 17, 19, 23};
        EXPECT_TRUE(buffer.copy(values.data(), values.size()));
        auto mapped = buffer.data(sizeof(values));
        EXPECT_EQ(std::memcmp(mapped.data, values.data(), sizeof(values)), 0);
        std::array<std::uint32_t, 4> replacement{29, 31, 37, 41};
        EXPECT_TRUE(mapped.copy(replacement.data(), replacement.size()));
        EXPECT_EQ(std::memcmp(mapped.data, replacement.data(), sizeof(replacement)), 0);
    }
    TEST_F(BufferVulkanIntegrationTest, PointerCopyPreservesOverlappingSourceAndRejectsInvalidCounts) {
        if (!host_coherent_available())
            GTEST_SKIP() << "HostVisible|HostCoherent memory unavailable";
        const std::array<std::uint32_t, 4> initial{3, 5, 7, 11};
        HostBuffer buffer(device, initial);
        auto mapped = buffer.data();
        auto* source = static_cast<std::uint32_t*>(mapped.data) + 1;
        ASSERT_TRUE(buffer.copy(source, std::size_t{3}));
        const std::array<std::uint32_t, 4> expected{5, 7, 11, 11};
        EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
        EXPECT_TRUE(buffer.copy(static_cast<std::uint32_t*>(nullptr), std::size_t{0}));
        EXPECT_FALSE(buffer.copy(source, std::numeric_limits<std::size_t>::max()));
        EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
    }
    TEST_F(BufferVulkanIntegrationTest, CopiesThroughDeviceLocalBufferOnGPU) {
        if (!host_coherent_available())
            GTEST_SKIP() << "HostVisible|HostCoherent memory unavailable";
        auto manager = make_manager();
        std::array<std::uint32_t, 4> expected{0, 1, 0xffffffffu, 42};
        HostBuffer source(device, expected.data(), expected.size());
        HostBuffer destination(device, sizeof(expected));
        vsl::DeviceLocalBuffer<vsl::MemoryType::TRANSFER_SOURCE> gpu(device, sizeof(expected));
        gpu.copy_by_buffer(manager, &source);
        destination.copy_by_buffer(manager, &gpu);
        auto data = destination.data(sizeof(expected));
        EXPECT_EQ(std::memcmp(data.data, expected.data(), sizeof(expected)), 0);
    }
    TEST_F(BufferVulkanIntegrationTest, ValueConstructorAndFactoryPreservePayload) {
        const std::array<std::uint32_t, 2> expected{71, 73};
        HostBuffer constructed(device, expected);
        auto payload = expected;
        auto generated = vsl::make_buffer<HostBuffer>(device, payload);
        auto first = constructed.data(sizeof(expected));
        auto second = generated.data(sizeof(expected));
        EXPECT_EQ(std::memcmp(first.data, expected.data(), sizeof(expected)), 0);
        EXPECT_EQ(std::memcmp(second.data, expected.data(), sizeof(expected)), 0);
    }
    TEST_F(BufferVulkanIntegrationTest, VariadicCopyStoresValuesConsecutively) {
        HostBuffer buffer(device, std::size_t{8});
        const std::uint32_t first = 17, second = 29;
        ASSERT_TRUE(buffer.copy(first, second));
        auto mapped = buffer.data(8);
        std::array<std::uint32_t, 2> actual{};
        std::memcpy(actual.data(), mapped.data, sizeof(actual));
        EXPECT_EQ(actual, (std::array<std::uint32_t, 2>{first, second}));
    }
    TEST(BufferCopyHelperTest, CheckedCopyAdvancesFromTheGivenOffsetAndStopsAtCapacity) {
        std::array<unsigned char, 12> storage{};
        storage.fill(0xa5);
        const std::uint32_t first = 0x10203040, second = 0x50607080;
        std::size_t offset = 4;
        ASSERT_TRUE(vsl::helper::copy_with_shift_offset(storage.data(), storage.size(), offset, first, second));
        EXPECT_EQ(offset, 12u);
        EXPECT_EQ(std::memcmp(storage.data() + 4, &first, sizeof(first)), 0);
        EXPECT_EQ(std::memcmp(storage.data() + 8, &second, sizeof(second)), 0);
        for (std::size_t i = 0; i < 4; ++i)
            EXPECT_EQ(storage[i], 0xa5);
        EXPECT_FALSE(vsl::helper::copy_with_shift_offset(storage.data(), storage.size(), offset, first));
        EXPECT_EQ(offset, 12u);
    }
    TEST(BufferCopyHelperTest, CheckedCapacityTestDoesNotWrapAtTheSizeLimit) {
        std::array<unsigned char, 4> storage{};
        const auto before = storage;
        std::size_t offset = std::numeric_limits<std::size_t>::max() - 1;
        EXPECT_FALSE(vsl::helper::copy_with_shift_offset(storage.data(),
                                                         std::numeric_limits<std::size_t>::max(), offset, std::uint32_t{7}));
        EXPECT_EQ(offset, std::numeric_limits<std::size_t>::max() - 1);
        EXPECT_EQ(storage, before);
    }
    void expect_overlapping_value_copy(bool checked) {
        struct Payload {
            std::uint32_t values[4];
            std::uint32_t tail;
        } payload{{1, 2, 3, 4}, 5};
        vsl::BufferAccessor::LocalBufferHolder mapped(nullptr, sizeof(payload.values), sizeof(std::uint32_t),
                                                      reinterpret_cast<std::byte*>(&payload) + sizeof(std::uint32_t));
        if (checked)
            EXPECT_TRUE(mapped.copy(payload.values));
        else
            mapped.uncheck_copy(payload.values);
        EXPECT_EQ(payload.values[0], 1u);
        EXPECT_EQ(payload.values[1], 1u);
        EXPECT_EQ(payload.values[2], 2u);
        EXPECT_EQ(payload.values[3], 3u);
    }

    TEST(BufferCopyHelperTest, CheckedMappedValueCopiesPreserveOverlappingSource) {
        expect_overlapping_value_copy(true);
    }

    TEST(BufferCopyHelperTest, UncheckedMappedValueCopiesPreserveOverlappingSource) {
        expect_overlapping_value_copy(false);
    }

    TEST(BufferCopyHelperTest, UncheckedCopyAppendsAndAdvancesItsOffset) {
        std::array<unsigned char, 12> storage{};
        const std::uint32_t first = 17, second = 29;
        std::size_t offset = 4;
        vsl::helper::uncheck_copy_with_shift_offset(storage.data(), offset, first);
        vsl::helper::uncheck_copy_with_shift_offset(storage.data(), offset, second);
        EXPECT_EQ(offset, 12u);
        EXPECT_EQ(std::memcmp(storage.data() + 4, &first, sizeof(first)), 0);
        EXPECT_EQ(std::memcmp(storage.data() + 8, &second, sizeof(second)), 0);
        for (std::size_t i = 0; i < 4; ++i)
            EXPECT_EQ(storage[i], 0);
    }
    TEST_F(BufferVulkanIntegrationTest, UncheckedVariadicCopyStoresValuesConsecutively) {
        HostBuffer buffer(device, std::size_t{8});
        const std::uint32_t first = 37, second = 41;
        buffer.uncheck_copy(first, second);
        auto mapped = buffer.data(8);
        const std::array<std::uint32_t, 2> expected{first, second};
        EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
    }
    TEST(BufferExceptionTest, MappingExceptionIncludesItsCause) {
        vsl::exceptions::MemoryNotHostVisibleException error("test trace");
        EXPECT_NE(error.reason.find("not host-visible"), std::string::npos);
        EXPECT_NE(error.reason.find("test trace"), std::string::npos);
        const auto location = std::source_location::current();
        vsl::exceptions::MemoryNotHostVisibleException located(location);
        EXPECT_NE(located.reason.find(std::to_string(location.line())), std::string::npos);
    }
} // namespace
namespace {
    TEST_F(BufferVulkanIntegrationTest, DeviceLocalValueAndPointerConstructorsUploadTheirPayloads) {
        const std::array<std::uint32_t, 4> expected{2, 7, 19, 43};
        auto manager = make_manager();
        vsl::DeviceLocalBuffer<vsl::MemoryType::TRANSFER_SOURCE> value(device, manager, expected);
        auto payload = expected;
        vsl::DeviceLocalBuffer<vsl::MemoryType::TRANSFER_SOURCE> pointer(device, manager, payload.data(),
                                                                         payload.size());
        HostBuffer readback(device, sizeof(expected));
        for (auto* buffer : {&value, &pointer}) {
            readback.copy_by_buffer(manager, buffer);
            auto mapped = readback.data(sizeof(expected));
            EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
        }
        EXPECT_THROW(value.data(), vsl::exceptions::MemoryNotHostVisibleException);
    }
    TEST_F(BufferVulkanIntegrationTest, UncheckedSingleValueAndMappedHolderCopiesRetainData) {
        HostBuffer buffer(device, std::size_t{16});
        const std::uint32_t first = 101, second = 103;
        buffer.uncheck_copy(first);
        {
            auto mapped = buffer.data(sizeof(first));
            std::uint32_t actual = 0;
            std::memcpy(&actual, mapped.data, sizeof(actual));
            EXPECT_EQ(actual, first);
            EXPECT_TRUE(mapped.copy(second));
            std::memcpy(&actual, mapped.data, sizeof(actual));
            EXPECT_EQ(actual, second);
            mapped.uncheck_copy(first);
            std::memcpy(&actual, mapped.data, sizeof(actual));
            EXPECT_EQ(actual, first);
            buffer.flush(mapped);
        }
    }
    TEST_F(BufferVulkanIntegrationTest, BufferTransferRejectsSourceLargerThanDestinationBeforeRecording) {
        const std::array<std::uint32_t, 4> values{1, 2, 3, 4};
        HostBuffer source(device, values), destination(device, std::size_t{4});
        auto manager = make_manager();
        // 予期しないvalidationエラーはfixtureで失敗させ、driverへの不正命令は抑止する。
        expect_validation_abort("");
        EXPECT_THROW(destination.copy_by_buffer(manager, &source), std::out_of_range);
        EXPECT_FALSE(has_validation_errors());
    }
    TEST_F(BufferVulkanIntegrationTest, MappedBufferTransferRejectsSourceLargerThanMapping) {
        const std::array<std::uint32_t, 4> values{1, 2, 3, 4};
        HostBuffer source(device, values), destination(device, std::size_t{32});
        auto mapping = destination.data(std::size_t{4}, 8);
        auto manager = make_manager();
        EXPECT_THROW(mapping.copy_by_buffer(manager, &source), std::out_of_range);
    }
    TEST_F(BufferVulkanIntegrationTest, MappedBufferTransferWritesOnlyAtTheMappingOffset) {
        const std::array<std::uint32_t, 8> initial{10, 20, 30, 40, 50, 60, 70, 80};
        const std::array<std::uint32_t, 2> payload{101, 102};
        HostBuffer source(device, payload), destination(device, initial);
        auto manager = make_manager();
        auto mapping = destination.data(sizeof(payload), 3 * sizeof(std::uint32_t));
        EXPECT_TRUE(mapping.copy_by_buffer(manager, &source));
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            vk::MemoryBarrier visible{.srcAccessMask = vk::AccessFlagBits::eTransferWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
            auto commands = manager.get_buffer();
            commands.data_->command_buffers[commands.get_current_buffer_idx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost, {}, visible, {}, {});
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        const std::array<std::uint32_t, 8> expected{10, 20, 30, 101, 102, 60, 70, 80};
        auto contents = destination.data();
        contents.invalidate(sizeof(expected));
        EXPECT_EQ(std::memcmp(contents.data, expected.data(), sizeof(expected)), 0);
    }
    TEST_F(BufferVulkanIntegrationTest, BufferTransferRejectsNullSourceAndOverlappingStorage) {
        HostBuffer buffer(device, std::size_t{16});
        auto alias = buffer;
        auto manager = make_manager();
        EXPECT_THROW(buffer.copy_by_buffer(manager, nullptr), std::invalid_argument);
        EXPECT_THROW(buffer.copy_by_buffer(manager, &alias), std::invalid_argument);
    }
    TEST_F(BufferVulkanIntegrationTest, BufferTransferRejectsBuffersWithoutTransferUsage) {
        vsl::Buffer<vsl::MemoryType::UNIFORM_BUFFER, vsl::MemoryProperty::HOST_VISIBLE> uniform(device, std::size_t{16});
        HostBuffer transfer(device, std::size_t{16});
        auto manager = make_manager();
        EXPECT_THROW(transfer.copy_by_buffer(manager, &uniform), std::invalid_argument);
        EXPECT_THROW(uniform.copy_by_buffer(manager, &transfer), std::invalid_argument);
    }
    TEST_F(BufferVulkanIntegrationTest, SuccessfulBufferTransferReturnsTrue) {
        std::array<std::uint32_t, 2> expected{47, 53};
        HostBuffer source(device, expected), destination(device, sizeof(expected));
        auto manager = make_manager();
        EXPECT_TRUE(destination.copy_by_buffer(manager, &source));
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            vk::MemoryBarrier host_visibility{};
            host_visibility.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            host_visibility.dstAccessMask = vk::AccessFlagBits::eHostRead;
            auto commands = manager.get_buffer();
            commands.data_->command_buffers[commands.get_current_buffer_idx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost, {}, host_visibility, {}, {});
        }
        device.data_->device.waitIdle();
        auto mapped = destination.data(sizeof(expected));
        EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
        EXPECT_TRUE(mapped.copy_by_buffer(manager, &source));
        EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
    }
    TEST_F(BufferVulkanIntegrationTest, DeviceLocalFactoryUploadsThePayloadWithThePublicConstructorOrder) {
        auto manager = make_manager();
        std::array<std::uint32_t, 4> expected{7, 31, 0xffffffffu, 0};
        auto value = vsl::make_buffer<vsl::DeviceLocalBuffer<vsl::MemoryType::TRANSFER_SOURCE>>(
            device, manager, expected);
        HostBuffer readback(device, sizeof(expected));
        readback.copy_by_buffer(manager, &value);
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            vk::MemoryBarrier host_visibility{};
            host_visibility.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            host_visibility.dstAccessMask = vk::AccessFlagBits::eHostRead;
            auto commands = manager.get_buffer();
            commands.data_->command_buffers[commands.get_current_buffer_idx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost, {}, host_visibility, {}, {});
        }
        device.data_->device.waitIdle();
        auto mapped = readback.data(sizeof(expected));
        EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
    }

} // namespace

namespace {
    TEST_F(BufferVulkanIntegrationTest, CheckedCopiesRespectTheMappedSizeRatherThanAllocationPadding) {
        const std::array<std::uint32_t, 2> initial{71, 73};
        HostBuffer buffer(device, initial);
        const std::array<std::uint32_t, 3> too_large{1, 2, 3};
        EXPECT_FALSE(buffer.copy(too_large.data(), too_large.size()));
        EXPECT_FALSE(buffer.copy(too_large));
        auto mapped = buffer.data(sizeof(initial));
        EXPECT_EQ(std::memcmp(mapped.data, initial.data(), sizeof(initial)), 0);
    }
    TEST_F(BufferVulkanIntegrationTest, MappedHolderCopiesStartAtItsPointerAndStayWithinItsRange) {
        std::array<unsigned char, 256> initial{};
        initial.fill(0xa5);
        HostBuffer buffer(device, initial);
        const auto alignment = physical.data_->props->limits.minMemoryMapAlignment;
        const auto map_offset = static_cast<std::size_t>(alignment);
        ASSERT_GT(map_offset, 0u);
        ASSERT_LE(map_offset + 8, initial.size());
        const std::uint32_t first = 17, second = 29;
        {
            auto mapped = buffer.data(8, map_offset);
            EXPECT_TRUE(mapped.copy(first, second));
            EXPECT_FALSE(mapped.copy(initial.data(), std::size_t{9}));
            EXPECT_FALSE(mapped.copy(initial));
            EXPECT_FALSE(mapped.copy(initial.data(), std::numeric_limits<std::size_t>::max()));
            EXPECT_TRUE(mapped.copy(static_cast<const std::uint32_t*>(nullptr), std::size_t{0}));
            const std::array<std::uint32_t, 2> expected{first, second};
            EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
            mapped.uncheck_copy(second, first);
            const std::array<std::uint32_t, 2> reversed{second, first};
            EXPECT_EQ(std::memcmp(mapped.data, reversed.data(), sizeof(reversed)), 0);
            EXPECT_EQ(mapped.offset, map_offset);
            EXPECT_EQ(mapped.size, 8u);
        }
        auto all = buffer.data(initial.size());
        for (std::size_t i = 0; i < initial.size(); ++i)
            if (i < map_offset || i >= map_offset + 8)
                EXPECT_EQ(static_cast<const unsigned char*>(all.data)[i], 0xa5) << i;
    }
    TEST(BufferCopyHelperTest, CheckedFailureKeepsTheSuccessfullyWrittenPrefix) {
        std::array<unsigned char, 6> storage{};
        const std::uint32_t first = 17, second = 29;
        std::size_t offset = 0;
        EXPECT_FALSE(vsl::helper::copy_with_shift_offset(storage.data(), storage.size(), offset, first, second));
        EXPECT_EQ(offset, 4u);
        EXPECT_EQ(std::memcmp(storage.data(), &first, sizeof(first)), 0);
        EXPECT_EQ(storage[4], 0);
        EXPECT_EQ(storage[5], 0);
    }
} // namespace

namespace {
    TEST_F(BufferVulkanIntegrationTest, OptionalManagerFactoryUploadsDeviceLocalValues) {
        const std::array<std::uint32_t, 4> expected{3, 17, 65537, 0xffffffffu};
        auto payload = expected;
        auto manager = make_manager();
        using Buffer = vsl::DeviceLocalBuffer<vsl::MemoryType::TRANSFER_SOURCE>;
        auto generated = vsl::make_buffer<Buffer>(device, payload, std::optional{manager});
        HostBuffer readback(device, sizeof(expected));
        readback.copy_by_buffer(manager, &generated);
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            vk::MemoryBarrier visible{};
            visible.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            visible.dstAccessMask = vk::AccessFlagBits::eHostRead;
            auto commands = manager.get_buffer();
            commands.data_->command_buffers[commands.get_current_buffer_idx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost, {}, visible, {}, {});
        }
        device.data_->device.waitIdle();
        auto mapped = readback.data(sizeof(expected));
        EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
    }
    TEST_F(BufferVulkanIntegrationTest, OptionalManagerFactoryReportsMissingManagerBeforeDeviceLocalMapping) {
        std::array<std::uint32_t, 2> payload{7, 11};
        try {
            auto buffer = vsl::make_buffer<vsl::DeviceLocalBuffer<vsl::MemoryType::TRANSFER_SOURCE>>(device, payload);
            FAIL() << "DeviceLocal initialization without a manager must fail";
        } catch (const vsl::exceptions::RuntimeException& error) {
            EXPECT_NE(error.reason.find("command manager"), std::string::npos);
        }
    }
    TEST_F(BufferVulkanIntegrationTest, OptionalManagerFactoryKeepsTheHostVisiblePathUsable) {
        std::array<std::uint32_t, 2> payload{19, 23};
        auto manager = make_manager();
        auto buffer = vsl::make_buffer<HostBuffer>(device, payload, manager);
        auto mapped = buffer.data(sizeof(payload));
        EXPECT_EQ(std::memcmp(mapped.data, payload.data(), sizeof(payload)), 0);
    }
} // namespace

namespace {

    bool BufferSharingVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }
    TEST_F(BufferSharingVulkanIntegrationTest, ConcurrentRequestWorksWithOneQueueFamilyAndPreservesMappedPayload) {
        if (!host_coherent_available())
            GTEST_SKIP() << "HostVisible|HostCoherent memory unavailable";
        ASSERT_EQ(device.data_->graphics_family, queue_family);
        ASSERT_EQ(device.data_->present_family, queue_family);
        const std::array<std::uint32_t, 4> expected{0, 17, 0xffffffffu, 101};
        ConcurrentHostBuffer buffer(device, expected);
        ASSERT_FALSE(has_validation_errors()) << "Buffer creation must pass validation before mapping";
        EXPECT_GE(buffer.size(), sizeof(expected));
        auto mapped = buffer.data(sizeof(expected));
        EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
    }
    TEST_F(BufferSharingVulkanIntegrationTest, ConcurrentDeviceLocalBufferTransfersPayloadUsingTheCreatedGraphicsQueue) {
        if (!host_coherent_available())
            GTEST_SKIP() << "HostVisible|HostCoherent memory unavailable";
        const std::array<std::uint32_t, 4> expected{13, 0, 0xffffffffu, 29};
        ConcurrentHostBuffer upload(device, expected), readback(device, sizeof(expected));
        vsl::DeviceLocalBuffer<vsl::MemoryType::TRANSFER_SOURCE, vsl::MemoryProperty::NONE, vsl::SharingMode::CONCURRENT> gpu(device, sizeof(expected));
        ASSERT_FALSE(has_validation_errors()) << "Do not submit buffers whose creation failed validation";
        auto manager = make_manager();
        EXPECT_TRUE(gpu.copy_by_buffer(manager, &upload));
        EXPECT_TRUE(readback.copy_by_buffer(manager, &gpu));
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            vk::MemoryBarrier host_visibility{};
            host_visibility.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            host_visibility.dstAccessMask = vk::AccessFlagBits::eHostRead;
            auto commands = manager.get_buffer();
            commands.data_->command_buffers[commands.get_current_buffer_idx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost, {}, host_visibility, {}, {});
        }
        device.data_->device.waitIdle();
        auto mapped = readback.data(sizeof(expected));
        EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
    }
} // namespace

namespace {
    TEST_F(BufferVulkanIntegrationTest, RequestedHostVisibleSubrangesFlushAndSurviveGPUCopy) {
        using Buffer = vsl::Buffer<vsl::MemoryType::TRANSFER_SOURCE, vsl::MemoryProperty::HOST_VISIBLE>;
        const auto atom = std::max<std::size_t>(4, physical.data_->props->limits.nonCoherentAtomSize);
        const std::size_t bytes = 2 * atom + 4;
        Buffer buffer(device, bytes);
        std::vector<unsigned char> expected(bytes, 0xa5);
        {
            auto all = buffer.data();
            std::memcpy(all.data, expected.data(), expected.size());
            all.flush();
        }
        for (const auto [offset, length] : {
                 std::pair<std::size_t, std::size_t>{3, 5}, {atom - 2, 8}, {atom + 3, 5}, {atom, atom}}) {
            auto part = buffer.data(length, offset);
            EXPECT_EQ(part.offset, offset);
            EXPECT_EQ(part.size, length);
            for (std::size_t i = 0; i < length; ++i) {
                expected[offset + i] = static_cast<unsigned char>(11 + offset + i);
                static_cast<unsigned char*>(part.data)[i] = expected[offset + i];
            }
            part.flush();
        }
        {
            auto tail = buffer.data(1, bytes - 1);
            expected.back() = 0x3f;
            *static_cast<unsigned char*>(tail.data) = expected.back();
            buffer.flush(tail);
        }
        auto manager = make_manager();
        HostBuffer readback(device, bytes);
        readback.copy_by_buffer(manager, &buffer);
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            vk::MemoryBarrier visible{};
            visible.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            visible.dstAccessMask = vk::AccessFlagBits::eHostRead;
            auto commands = manager.get_buffer();
            commands.data_->command_buffers[commands.get_current_buffer_idx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost, {}, visible, {}, {});
        }
        device.data_->device.waitIdle();
        auto mapped = readback.data();
        EXPECT_EQ(std::memcmp(mapped.data, expected.data(), expected.size()), 0);
    }
    TEST_F(BufferVulkanIntegrationTest, MappingRejectsRangesOutsideRequestedStorage) {
        HostBuffer buffer(device, std::size_t{19});
        EXPECT_EQ(buffer.size(), 19u);
        {
            auto mapped = buffer.data(buffer.size());
            EXPECT_EQ(mapped.size, 19u);
        }
        EXPECT_THROW(buffer.data(20), vsl::exceptions::RuntimeException);
        EXPECT_THROW(buffer.data(1, 19), vsl::exceptions::RuntimeException);
    }
    TEST_F(BufferVulkanIntegrationTest, DefaultMappingSizeExcludesTheRequestedOffset) {
        const auto offset = physical.data_->props->limits.minMemoryMapAlignment;
        HostBuffer buffer(device, std::size_t{offset * 2 + 1});
        auto mapped = buffer.data(std::nullopt, offset);
        EXPECT_EQ(mapped.offset, offset);
        EXPECT_EQ(mapped.size, offset + 1);
    }
} // namespace

namespace {
    TEST_F(BufferVulkanIntegrationTest, HostVisibleScalarConstructorFlushesBeforeUnmapping) {
        using Buffer = vsl::Buffer<vsl::MemoryType::UNIFORM_BUFFER, vsl::MemoryProperty::HOST_VISIBLE>;
        const float expected = 1.25f;
        Buffer buffer(device, expected);
        EXPECT_EQ(buffer.size(), sizeof(expected));
        auto mapped = buffer.data();
        float actual = 0;
        std::memcpy(&actual, mapped.data, sizeof(actual));
        EXPECT_FLOAT_EQ(actual, expected);
    }
    TEST_F(BufferVulkanIntegrationTest, HostVisibleValuePointerAndFactoryPayloadsReachGPUTransfers) {
        using Buffer = vsl::Buffer<vsl::MemoryType::TRANSFER_SOURCE, vsl::MemoryProperty::HOST_VISIBLE>;
        const std::array<std::uint32_t, 4> expected{17, 23, 0xffffffffu, 0};
        Buffer value(device, expected);
        auto payload = expected;
        Buffer pointer(device, payload.data(), payload.size());
        auto generated = vsl::make_buffer<Buffer>(device, payload);
        auto manager = make_manager();
        HostBuffer readback(device, sizeof(expected));
        for (auto* source : {&value, &pointer, &generated}) {
            readback.copy_by_buffer(manager, source);
            {
                auto phase = manager.start_phase<vsl::ComputePhase>();
                vk::MemoryBarrier visible{};
                visible.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
                visible.dstAccessMask = vk::AccessFlagBits::eHostRead;
                auto commands = manager.get_buffer();
                commands.data_->command_buffers[commands.get_current_buffer_idx()].pipelineBarrier(
                    vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost, {}, visible, {}, {});
            }
            device.data_->device.waitIdle();
            auto mapped = readback.data();
            EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
        }
    }
} // namespace

namespace {

    void FlushTrackingBuffer::flush(LocalBufferHolder& holder) {
        EXPECT_EQ(holder.parent, this);
        ++flush_calls;
        Base::flush(holder);
    }
    TEST_F(BufferVulkanIntegrationTest, ScopedCopiesFlushWhileTheMappingIsAlive) {
        FlushTrackingBuffer buffer(device, std::size_t{16});
        const std::uint32_t first = 17, second = 29;
        EXPECT_TRUE(buffer.copy(first));
        EXPECT_EQ(buffer.flush_calls, 1);
        std::array<std::uint32_t, 2> values{first, second};
        EXPECT_TRUE(buffer.copy(values.data(), values.size()));
        EXPECT_EQ(buffer.flush_calls, 2);
        buffer.uncheck_copy(second, first);
        EXPECT_EQ(buffer.flush_calls, 3);
        auto mapped = buffer.data(sizeof(values));
        EXPECT_EQ(std::memcmp(mapped.data, (std::array<std::uint32_t, 2>{second, first}).data(), sizeof(values)), 0);
    }
    TEST_F(BufferVulkanIntegrationTest, FailedCopiesFlushWrittenPrefixesButRejectedAndEmptyCopiesDoNotFlush) {
        FlushTrackingBuffer buffer(device, std::size_t{6});
        {
            auto mapped = buffer.data();
            std::memset(mapped.data, 0xa5, mapped.size);
            mapped.flush();
        }
        buffer.flush_calls = 0;
        const std::uint32_t first = 17, second = 29;
        EXPECT_FALSE(buffer.copy(first, second));
        EXPECT_EQ(buffer.flush_calls, 1);
        {
            auto mapped = buffer.data();
            EXPECT_EQ(std::memcmp(mapped.data, &first, sizeof(first)), 0);
            EXPECT_EQ(static_cast<const unsigned char*>(mapped.data)[4], 0xa5);
            EXPECT_EQ(static_cast<const unsigned char*>(mapped.data)[5], 0xa5);
        }
        std::array<std::uint32_t, 2> too_large{3, 5};
        EXPECT_FALSE(buffer.copy(too_large));
        EXPECT_FALSE(buffer.copy(too_large.data(), too_large.size()));
        EXPECT_TRUE(buffer.copy(static_cast<const std::uint32_t*>(nullptr), std::size_t{0}));
        EXPECT_TRUE(buffer.copy());
        buffer.uncheck_copy();
        EXPECT_EQ(buffer.flush_calls, 1);
    }
    TEST_F(BufferVulkanIntegrationTest, MappedHolderCopiesKeepExplicitFlushControl) {
        FlushTrackingBuffer buffer(device, std::size_t{16});
        auto mapped = buffer.data();
        EXPECT_TRUE(mapped.copy(std::uint32_t{17}));
        mapped.uncheck_copy(std::uint32_t{29});
        EXPECT_EQ(buffer.flush_calls, 0);
        mapped.flush();
        EXPECT_EQ(buffer.flush_calls, 0);
        buffer.flush(mapped);
        EXPECT_EQ(buffer.flush_calls, 1);
        EXPECT_EQ(*static_cast<const std::uint32_t*>(mapped.data), 29u);
    }
} // namespace

namespace {
    TEST_F(BufferVulkanIntegrationTest, PointerCopiesUseSignedAndNarrowIntegralCountsRatherThanPackingArguments) {
        const std::array<std::uint32_t, 3> expected{17, 23, 0xffffffffu};
        HostBuffer buffer(device, std::size_t{32});
        auto verify = [&]<typename Count>(Count count) {
            ASSERT_TRUE(buffer.copy(expected.data(), count));
            {
                auto mapped = buffer.data(sizeof(expected));
                EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
            }
            auto mapped = buffer.data(sizeof(expected));
            std::memset(mapped.data, 0, mapped.size);
            ASSERT_TRUE(mapped.copy(expected.data(), count));
            EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
        };
        verify(3);
        verify(short{3});
        verify(3u);
        verify(3L);
        verify(std::size_t{3});
        const std::uint32_t raw[]{17, 23, 0xffffffffu};
        ASSERT_TRUE(buffer.copy(raw, 3));
        auto mapped = buffer.data(sizeof(raw));
        EXPECT_EQ(std::memcmp(mapped.data, raw, sizeof(raw)), 0);
    }
    TEST_F(BufferVulkanIntegrationTest, NegativeAndOversizedCountsLeaveStorageUntouchedAndNullEmptyCopiesSucceed) {
        const std::array<std::uint32_t, 4> initial{17, 23, 29, 31};
        HostBuffer buffer(device, initial);
        EXPECT_FALSE(buffer.copy(initial.data(), -1));
        EXPECT_FALSE(buffer.copy(initial.data(), std::numeric_limits<long>::min()));
        EXPECT_FALSE(buffer.copy(initial.data(), 5u));
        EXPECT_TRUE(buffer.copy(static_cast<const std::uint32_t*>(nullptr), 0));
        auto mapped = buffer.data();
        EXPECT_FALSE(mapped.copy(initial.data(), short{-1}));
        EXPECT_FALSE(mapped.copy(initial.data(), 5));
        EXPECT_TRUE(mapped.copy(static_cast<const std::uint32_t*>(nullptr), 0));
        EXPECT_EQ(std::memcmp(mapped.data, initial.data(), sizeof(initial)), 0);
    }
} // namespace

namespace {
    TEST_F(BufferVulkanIntegrationTest, UnscopedEnumCountsCopyPointerAndCArrayElementsWithoutSerializingTheCount) {
        const std::array<std::uint32_t, 4> initial{0xa5a5a5a5u, 0xa5a5a5a5u, 0xa5a5a5a5u, 0xa5a5a5a5u};
        const std::array<std::uint32_t, 3> source{17, 23, 29};
        const std::array<std::uint32_t, 4> expected{17, 23, 29, 0xa5a5a5a5u};
        HostBuffer buffer(device, initial);
        EXPECT_TRUE(buffer.copy(source.data(), THREE_ELEMENTS));
        {
            auto mapped = buffer.data();
            EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
        }
        ASSERT_TRUE(buffer.copy(initial));
        const std::uint32_t raw[]{17, 23, 29};
        auto mapped = buffer.data();
        EXPECT_TRUE(mapped.copy(raw, THREE_ELEMENTS));
        EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
    }
    TEST_F(BufferVulkanIntegrationTest, NegativeAndEmptyEnumCountsKeepExistingStorageIntact) {
        const std::array<std::uint32_t, 4> initial{17, 23, 29, 31};
        HostBuffer buffer(device, initial);
        EXPECT_FALSE(buffer.copy(initial.data(), NEGATIVE_ELEMENTS));
        EXPECT_TRUE(buffer.copy(static_cast<const std::uint32_t*>(nullptr), NO_ELEMENTS));
        auto mapped = buffer.data();
        EXPECT_FALSE(mapped.copy(initial.data(), NEGATIVE_ELEMENTS));
        EXPECT_TRUE(mapped.copy(static_cast<const std::uint32_t*>(nullptr), NO_ELEMENTS));
        EXPECT_EQ(std::memcmp(mapped.data, initial.data(), sizeof(initial)), 0);
    }
} // namespace

namespace {
    TEST(BufferCompilationTest, DeviceLocalPointerUploadCompilesWithStrictPosixHeaders) {
        const auto result = vsl_test::compile_probe(R"cpp(
#define _POSIX_C_SOURCE 200809L
#include <VSL/vulkan/buffer.hpp>
void upload(vsl::LogicalDeviceAccessor device, vsl::CommandManager manager, const unsigned *values) {
    vsl::DeviceLocalBuffer<> buffer(device, manager, values, std::size_t{3});
}
)cpp");
        EXPECT_EQ(result.status, 0) << result.diagnostics;
    }
} // namespace

namespace {
    TEST_F(BufferVulkanIntegrationTest, HostVisibleArrayConstructionRejectsByteCountOverflowInsteadOfWrapping) {
        const std::uint32_t source = 17;
        const auto overflowing_count = std::numeric_limits<std::size_t>::max() / sizeof(source) + 2;
        // 旧実装では乗算が1要素分へ折り返す値で、実際の確保とコピーを有効な範囲に保つ。
        EXPECT_THROW((HostBuffer(device, &source, overflowing_count)), vsl::exceptions::RuntimeException);
    }
    TEST_F(BufferVulkanIntegrationTest, DeviceLocalArrayConstructionRejectsByteCountOverflowInsteadOfWrapping) {
        const std::uint32_t source = 23;
        const auto overflowing_count = std::numeric_limits<std::size_t>::max() / sizeof(source) + 2;
        auto manager = make_manager();
        using Buffer = vsl::DeviceLocalBuffer<vsl::MemoryType::TRANSFER_SOURCE>;
        EXPECT_THROW((Buffer(device, manager, &source, overflowing_count)), vsl::exceptions::RuntimeException);
    }
    TEST_F(BufferVulkanIntegrationTest, ArrayConstructionRejectsZeroElementsBeforeCreatingNativeBuffers) {
        const std::uint32_t source = 29;
        EXPECT_THROW((HostBuffer(device, &source, std::size_t{0})), vsl::exceptions::RuntimeException);
        auto manager = make_manager();
        using Buffer = vsl::DeviceLocalBuffer<vsl::MemoryType::TRANSFER_SOURCE>;
        EXPECT_THROW((Buffer(device, manager, &source, std::size_t{0})), vsl::exceptions::RuntimeException);
    }
} // namespace

namespace {
    TEST_F(BufferVulkanIntegrationTest, UnavailableMemoryPropertiesReportTheMissingTypeInsteadOfUsingIndexZero) {
        const auto requested = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eLazilyAllocated;
        const auto& properties = *physical.data_->mem_props;
        for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i)
            ASSERT_NE(properties.memoryTypes[i].propertyFlags & requested, requested);
        using Buffer = vsl::Buffer<vsl::MemoryType::TRANSFER_SOURCE,
                                   vsl::MemoryProperty::HOST_VISIBLE | vsl::MemoryProperty::LAZILY_ALLOCATED>;
        try {
            Buffer buffer(device, std::size_t{16});
            FAIL() << "No memory type satisfies the requested property combination";
        } catch (const vsl::exceptions::RuntimeException& error) {
            EXPECT_NE(error.reason.find("memory type"), std::string::npos) << error.reason;
        }
    }
    TEST_F(BufferVulkanIntegrationTest, FailedMemorySelectionDoesNotPreventLaterValidAllocations) {
        using Unsupported = vsl::Buffer<vsl::MemoryType::TRANSFER_SOURCE,
                                        vsl::MemoryProperty::HOST_VISIBLE | vsl::MemoryProperty::LAZILY_ALLOCATED>;
        EXPECT_THROW((Unsupported(device, std::size_t{16})), vsl::exceptions::RuntimeException);
        const std::array<std::uint32_t, 2> payload{17, 23};
        HostBuffer buffer(device, payload);
        auto mapped = buffer.data(sizeof(payload));
        EXPECT_EQ(std::memcmp(mapped.data, payload.data(), sizeof(payload)), 0);
    }
} // namespace

namespace {
    TEST(BufferCompilationTest, HostVisibleNonCoherentValueConstructorCanFlush) {
        const auto result = vsl_test::compile_probe(R"cpp(
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vsl.hpp>
void probe(vsl::LogicalDeviceAccessor device){
    vsl::Buffer<vsl::MemoryType::UNIFORM_BUFFER,vsl::MemoryProperty::HOST_VISIBLE> buffer(device,1.0f);
}
)cpp");
        EXPECT_EQ(result.status, 0) << result.diagnostics;
    }
} // namespace

namespace {

    bool PersistentBufferVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }

    TEST_F(PersistentBufferVulkanIntegrationTest, ReusesMappingAndReadsBoundedRangesAfterGPUCompletion) {
        auto manager = make_manager();
        ReadbackBuffer destination(device, WIDTH * HEIGHT * 4);
        auto mapping = destination.data();
        const std::byte* address = nullptr;
        for (unsigned frame = 0; frame < 8; ++frame) {
            VSL_NAMESPACE::ComputePhase phase(manager);
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{frame % 2 ? 0.0f : 1.0f, frame % 2 ? 1.0f : 0.0f, 0, 1}})
                  << VSL_NAMESPACE::command::RenderPassEnd{}
                  << VSL_NAMESPACE::command::ImageReadback(&destination, *color, VSL_NAMESPACE::ImageLayout::GENERAL);
            ASSERT_EQ(phase.submit(), VSL_NAMESPACE::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
            auto pixels = mapping.read(4);
            EXPECT_EQ(pixels[0], frame % 2 ? std::byte{0} : std::byte{255});
            EXPECT_EQ(pixels[1], frame % 2 ? std::byte{255} : std::byte{0});
            EXPECT_EQ(pixels[3], std::byte{255});
            if (address)
                EXPECT_EQ(pixels.data(), address);
            address = pixels.data();
        }
        auto alias = destination;
        auto alias_mapping = alias.data();
        EXPECT_EQ(alias_mapping.read(4).data(), address);
        EXPECT_EQ(alias_mapping.read(3, 1)[2], std::byte{255});
        EXPECT_TRUE(alias_mapping.read(0, alias.requested_size()).empty());
        EXPECT_THROW(mapping.read(1, destination.requested_size()), std::out_of_range);
        EXPECT_THROW(mapping.read(SIZE_MAX, 1), std::out_of_range);
        EXPECT_THROW((ReadbackBuffer(device, std::size_t{0})), std::invalid_argument);
        vsl::Buffer<vsl::MemoryType::STORAGE_BUFFER, vsl::MemoryProperty::HOST_VISIBLE> storage(device, WIDTH * HEIGHT * 4);
        EXPECT_THROW((VSL_NAMESPACE::command::ImageReadback(&storage, *color, VSL_NAMESPACE::ImageLayout::GENERAL).copy.description()), std::invalid_argument);
    }
    TEST_F(PersistentBufferVulkanIntegrationTest, InvalidationPathAlignsPartialRangesAndKeepsMapping) {
        auto manager = make_manager();
        ReadbackBuffer destination(device, WIDTH * HEIGHT * 4);
        auto mapping = destination.data();
        {
            VSL_NAMESPACE::ComputePhase phase(manager);
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{1, 0, 0, 1}})
                  << VSL_NAMESPACE::command::RenderPassEnd{}
                  << VSL_NAMESPACE::command::ImageReadback(&destination, *color, VSL_NAMESPACE::ImageLayout::GENERAL);
            phase.submit();
            ASSERT_TRUE(phase.completion().wait());
        }
        // コヒーレントメモリしかない環境でも、キャッシュ無効化範囲のアラインメントを確かめる。
        // 実際の非コヒーレントメモリの動作を再現する検証ではない。
        destination.data_->properties = vsl::MemoryProperty::HOST_VISIBLE;
        destination.data_->atom_size = physical.data_->device.getProperties().limits.nonCoherentAtomSize;
        auto partial = mapping.read(3, 1);
        EXPECT_EQ(partial[0], std::byte{0});
        EXPECT_EQ(partial[2], std::byte{255});
        EXPECT_EQ(mapping.read(1, destination.requested_size() - 1)[0], std::byte{255});
    }
    TEST_F(PersistentBufferVulkanIntegrationTest, RecordedCommandKeepsBufferAliveUntilCompletionOwnerReleasesIt) {
        auto manager = make_manager();
        std::weak_ptr<VSL_NAMESPACE::impl::BufferImpl> weak;
        {
            VSL_NAMESPACE::ComputePhase phase(manager);
            {
                ReadbackBuffer destination(device, WIDTH * HEIGHT * 4);
                auto mapping = destination.data();
                weak = destination.data_;
                phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer) << VSL_NAMESPACE::command::RenderPassEnd{}
                      << VSL_NAMESPACE::command::ImageReadback(&destination, *color, VSL_NAMESPACE::ImageLayout::GENERAL);
            }
            EXPECT_FALSE(weak.expired());
            phase.submit();
            EXPECT_TRUE(phase.completion().wait());
        }
        {
            VSL_NAMESPACE::ComputePhase next(manager);
            next.submit();
            EXPECT_TRUE(next.completion().wait());
        }
        EXPECT_TRUE(weak.expired());
    }
    TEST_F(PersistentBufferVulkanIntegrationTest, IndependentManagerHasOwnBuffersOnSameDeviceAndQueue) {
        auto producer = make_manager();
        auto transfer = producer.independent();
        EXPECT_EQ(transfer.get_device(), producer.get_device());
        EXPECT_NE(transfer.get_pool().data_, producer.get_pool().data_);
        EXPECT_EQ(transfer.get_buffer().get_size(), 1u);
        EXPECT_EQ(transfer.data_->graphics_queue, producer.data_->graphics_queue);
        auto producer_phase = producer.start_phase<VSL_NAMESPACE::ComputePhase>();
        {
            auto transfer_phase = transfer.start_phase<VSL_NAMESPACE::ComputePhase>();
            transfer_phase.submit();
            EXPECT_TRUE(transfer_phase.completion().wait());
        }
        EXPECT_EQ(producer_phase.state, VSL_NAMESPACE::ComputePhase::State::WAIT_SUBMIT);
        producer_phase.cancel();
        EXPECT_THROW(producer.independent(0), std::invalid_argument);
    }
} // namespace

namespace {
    TEST_F(BufferVulkanIntegrationTest, SharedMappingsSupportUnalignedViewsWithoutRemapping) {
        using Buffer = vsl::Buffer<vsl::MemoryType::TRANSFER_SOURCE, vsl::MemoryProperty::HOST_VISIBLE>;
        Buffer buffer(device, std::size_t{33});
        EXPECT_EQ(buffer.requested_size(), 33u);
        EXPECT_FALSE(buffer.data_->mapped);
        auto first = buffer.data();
        auto alias = buffer;
        auto partial = alias.data(7, 1);
        EXPECT_EQ(partial.data, static_cast<std::byte*>(first.data) + 1);
        const std::array payload{std::byte{17}, std::byte{29}, std::byte{43}};
        partial.write(payload, 2);
        EXPECT_EQ(first.read(1, 3)[0], payload[0]);
        EXPECT_EQ(first.read(1, 5)[0], payload[2]);
        auto remaining = buffer.data(std::nullopt, 3);
        EXPECT_EQ(remaining.size, 30u);
        EXPECT_EQ(remaining.data, static_cast<std::byte*>(first.data) + 3);
        EXPECT_TRUE(buffer.data(0, 33).read(0).empty());
        EXPECT_THROW(buffer.data(1, 33), vsl::exceptions::RuntimeException);
        EXPECT_THROW(buffer.data(std::nullopt, SIZE_MAX), vsl::exceptions::RuntimeException);
        EXPECT_THROW(partial.read(1, 7), std::out_of_range);
        EXPECT_THROW(partial.write(payload, 6), std::out_of_range);
        EXPECT_THROW(partial.flush(SIZE_MAX, 1), std::out_of_range);
        EXPECT_THROW(partial.invalidate(1, SIZE_MAX), std::out_of_range);
        auto moved = std::move(partial);
        EXPECT_THROW(partial.read(0), std::logic_error);
        EXPECT_THROW(partial.write({}), std::logic_error);
        EXPECT_EQ(moved.read(1, 2)[0], payload[0]);
    }
    TEST_F(BufferVulkanIntegrationTest, FlushAndInvalidateAlignPartialViewsAndAllocationTail) {
        using Buffer = vsl::Buffer<vsl::MemoryType::TRANSFER_SOURCE, vsl::MemoryProperty::HOST_VISIBLE>;
        Buffer buffer(device, std::size_t{33});
        auto mapped = buffer.data();
        // 実際の非コヒーレントheapの可視性ではなく、範囲の整列を検証する。
        buffer.data_->properties = vsl::MemoryProperty::HOST_VISIBLE;
        auto partial = buffer.data(31, 1);
        const std::array payload{std::byte{71}, std::byte{73}};
        partial.write(payload, 29);
        partial.flush(1, 30);
        partial.invalidate(1, 30);
        mapped.write(std::span(payload).first(1), 32);
        EXPECT_EQ(mapped.read(1, 30)[0], payload[0]);
        EXPECT_EQ(mapped.read(1, 31)[0], payload[1]);
        EXPECT_EQ(mapped.read(1, 32)[0], payload[0]);
        EXPECT_NO_THROW(mapped.write({}, 33));
        EXPECT_NO_THROW(mapped.flush(0, 33));
    }
    TEST_F(BufferVulkanIntegrationTest, MappingKeepsItsOriginalStorageWhenBufferIsReassigned) {
        using Buffer = vsl::Buffer<vsl::MemoryType::TRANSFER_SOURCE, vsl::MemoryProperty::HOST_VISIBLE>;
        Buffer buffer(device, std::size_t{4});
        auto mapping = buffer.data();
        const auto original = buffer.data_;
        buffer = Buffer(device, std::size_t{8});
        EXPECT_NE(buffer.data_, original);
        const std::array payload{std::byte{101}};
        mapping.write(payload);
        EXPECT_EQ(mapping.read(1)[0], payload[0]);
        EXPECT_EQ(mapping.data, original->mapped);
    }
    TEST_F(BufferVulkanIntegrationTest, RejectsUnavailableRequiredMemoryInsteadOfSelectingAnUnrelatedType) {
        const auto properties = physical.data_->device.getMemoryProperties();
        auto selected = vk::MemoryPropertyFlags{};
        for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i)
            selected |= properties.memoryTypes[i].propertyFlags;
        if (selected & vk::MemoryPropertyFlagBits::eProtected)
            GTEST_SKIP() << "This device has protected memory";
        using ProtectedBuffer = vsl::Buffer<vsl::MemoryType::TRANSFER_SOURCE, vsl::MemoryProperty::HOST_VISIBLE | vsl::MemoryProperty::PROTECTED>;
        EXPECT_THROW((ProtectedBuffer(device, std::size_t{4})), vsl::exceptions::RuntimeException);
    }
} // namespace
