#include "support/vulkan_fixture.hpp"
#include <array>
#include <limits>
#include "support/compile_probe.hpp"

namespace {
using BufferVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
using HostBuffer = vsl::StagingBuffer<vsl::MemoryType::TransferDestination>;
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
TEST_F(BufferVulkanIntegrationTest, CopiesThroughDeviceLocalBufferOnGpu) {
    if (!host_coherent_available())
        GTEST_SKIP() << "HostVisible|HostCoherent memory unavailable";
    auto manager = make_manager();
    std::array<std::uint32_t, 4> expected{0, 1, 0xffffffffu, 42};
    HostBuffer source(device, expected.data(), expected.size());
    HostBuffer destination(device, sizeof(expected));
    vsl::DeviceLocalBuffer<vsl::MemoryType::TransferSource> gpu(device, sizeof(expected));
    gpu.copyByBuffer(manager, &source);
    destination.copyByBuffer(manager, &gpu);
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
    for (std::size_t i = 0; i < 4; ++i) EXPECT_EQ(storage[i], 0xa5);
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
TEST(BufferCopyHelperTest, UncheckedCopyAppendsAndAdvancesItsOffset) {
    std::array<unsigned char, 12> storage{};
    const std::uint32_t first = 17, second = 29;
    std::size_t offset = 4;
    vsl::helper::uncheck_copy_with_shift_offset(storage.data(), offset, first);
    vsl::helper::uncheck_copy_with_shift_offset(storage.data(), offset, second);
    EXPECT_EQ(offset, 12u);
    EXPECT_EQ(std::memcmp(storage.data() + 4, &first, sizeof(first)), 0);
    EXPECT_EQ(std::memcmp(storage.data() + 8, &second, sizeof(second)), 0);
    for (std::size_t i = 0; i < 4; ++i) EXPECT_EQ(storage[i], 0);
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
    vsl::DeviceLocalBuffer<vsl::MemoryType::TransferSource> value(device, manager, expected);
    auto payload = expected;
    vsl::DeviceLocalBuffer<vsl::MemoryType::TransferSource> pointer(device, manager, payload.data(),
                                                                    payload.size());
    HostBuffer readback(device, sizeof(expected));
    for (auto *buffer : {&value, &pointer}) {
        readback.copyByBuffer(manager, buffer);
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
TEST_F(BufferVulkanIntegrationTest, DISABLED_SuccessfulBufferTransferReturnsTrue) {
    std::array<std::uint32_t, 2> expected{47, 53};
    HostBuffer source(device, expected), destination(device, sizeof(expected));
    auto manager = make_manager();
    EXPECT_TRUE(destination.copyByBuffer(manager, &source));
    auto mapped = destination.data(sizeof(expected));
    EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
    EXPECT_TRUE(mapped.copyByBuffer(manager, &source));
}
TEST(BufferCompilationTest, DISABLED_DeviceLocalFactoryUsesThePublicConstructorArgumentOrder) {
    const auto result = vsl_test::compile_probe(R"cpp(
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vsl.hpp>
void probe(vsl::LogicalDeviceAccessor device,vsl::CommandManager manager){
    float value=1;
    auto buffer=vsl::make_buffer<vsl::DeviceLocalBuffer<vsl::MemoryType::UniformBuffer>>(device,manager,value);
}
)cpp");
    EXPECT_EQ(result.status, 0) << result.diagnostics;
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
    const auto alignment = physical._data->props->limits.minMemoryMapAlignment;
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
        EXPECT_TRUE(mapped.copy(static_cast<const std::uint32_t *>(nullptr), std::size_t{0}));
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
            EXPECT_EQ(static_cast<const unsigned char *>(all.data)[i], 0xa5) << i;
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
TEST_F(BufferVulkanIntegrationTest, RequestedHostVisibleSubrangesFlushAndSurviveGpuCopy) {
    using Buffer = vsl::Buffer<vsl::MemoryType::TransferSource, vsl::MemoryProperty::HostVisible>;
    const auto atom = std::max<std::size_t>(4, physical._data->props->limits.nonCoherentAtomSize);
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
            static_cast<unsigned char *>(part.data)[i] = expected[offset + i];
        }
        part.flush();
    }
    {
        auto tail = buffer.data(1, bytes - 1);
        expected.back() = 0x3f;
        *static_cast<unsigned char *>(tail.data) = expected.back();
        buffer.flush(tail);
    }
    auto manager = make_manager();
    HostBuffer readback(device, bytes);
    readback.copyByBuffer(manager, &buffer);
    {
        auto phase = manager.startPhase<vsl::ComputePhase>();
        vk::MemoryBarrier visible{};
        visible.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        visible.dstAccessMask = vk::AccessFlagBits::eHostRead;
        auto commands = manager.getBuffer();
        commands._data->commandBuffers[commands.getCurrentBufferIdx()].pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost, {}, visible, {}, {});
    }
    device._data->device.waitIdle();
    auto mapped = readback.data();
    EXPECT_EQ(std::memcmp(mapped.data, expected.data(), expected.size()), 0);
}
TEST_F(BufferVulkanIntegrationTest, MappingRejectsRangesOutsideRequestedStorage) {
    HostBuffer buffer(device, std::size_t{19});
    EXPECT_EQ(buffer.size(), 19u);
    { auto mapped = buffer.data(buffer.size()); EXPECT_EQ(mapped.size, 19u); }
    EXPECT_THROW(buffer.data(20), vsl::exceptions::RuntimeException);
    EXPECT_THROW(buffer.data(1, 19), vsl::exceptions::RuntimeException);
}
TEST_F(BufferVulkanIntegrationTest, DefaultMappingSizeExcludesTheRequestedOffset) {
    const auto offset = physical._data->props->limits.minMemoryMapAlignment;
    HostBuffer buffer(device, std::size_t{offset * 2 + 1});
    auto mapped = buffer.data(std::nullopt, offset);
    EXPECT_EQ(mapped.offset, offset);
    EXPECT_EQ(mapped.size, offset + 1);
}
} // namespace

namespace {
TEST_F(BufferVulkanIntegrationTest, HostVisibleScalarConstructorFlushesBeforeUnmapping) {
    using Buffer = vsl::Buffer<vsl::MemoryType::UniformBuffer, vsl::MemoryProperty::HostVisible>;
    const float expected = 1.25f;
    Buffer buffer(device, expected);
    EXPECT_EQ(buffer.size(), sizeof(expected));
    auto mapped = buffer.data();
    float actual = 0;
    std::memcpy(&actual, mapped.data, sizeof(actual));
    EXPECT_FLOAT_EQ(actual, expected);
}
TEST_F(BufferVulkanIntegrationTest, HostVisibleValuePointerAndFactoryPayloadsReachGpuTransfers) {
    using Buffer = vsl::Buffer<vsl::MemoryType::TransferSource, vsl::MemoryProperty::HostVisible>;
    const std::array<std::uint32_t, 4> expected{17, 23, 0xffffffffu, 0};
    Buffer value(device, expected);
    auto payload = expected;
    Buffer pointer(device, payload.data(), payload.size());
    auto generated = vsl::make_buffer<Buffer>(device, payload);
    auto manager = make_manager();
    HostBuffer readback(device, sizeof(expected));
    for (auto *source : {&value, &pointer, &generated}) {
        readback.copyByBuffer(manager, source);
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            vk::MemoryBarrier visible{};
            visible.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            visible.dstAccessMask = vk::AccessFlagBits::eHostRead;
            auto commands = manager.getBuffer();
            commands._data->commandBuffers[commands.getCurrentBufferIdx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost, {}, visible, {}, {});
        }
        device._data->device.waitIdle();
        auto mapped = readback.data();
        EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
    }
}
} // namespace

namespace {
struct FlushTrackingBuffer : vsl::Buffer<vsl::MemoryType::TransferSource, vsl::MemoryProperty::HostVisible> {
    using Base = vsl::Buffer<vsl::MemoryType::TransferSource, vsl::MemoryProperty::HostVisible>;
    using Base::Base;
    int flush_calls = 0;
    void flush(LocalBufferHolder &holder) override {
        EXPECT_EQ(holder.parent, this);
        ++flush_calls;
        Base::flush(holder);
    }
};
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
        EXPECT_EQ(static_cast<const unsigned char *>(mapped.data)[4], 0xa5);
        EXPECT_EQ(static_cast<const unsigned char *>(mapped.data)[5], 0xa5);
    }
    std::array<std::uint32_t, 2> too_large{3, 5};
    EXPECT_FALSE(buffer.copy(too_large));
    EXPECT_FALSE(buffer.copy(too_large.data(), too_large.size()));
    EXPECT_TRUE(buffer.copy(static_cast<const std::uint32_t *>(nullptr), std::size_t{0}));
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
    EXPECT_EQ(buffer.flush_calls, 1);
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
    // The old multiplication wraps to one element, keeping the reproduction's native allocation valid.
    EXPECT_THROW((HostBuffer(device, &source, overflowing_count)), vsl::exceptions::RuntimeException);
}
TEST_F(BufferVulkanIntegrationTest, DeviceLocalArrayConstructionRejectsByteCountOverflowInsteadOfWrapping) {
    const std::uint32_t source = 23;
    const auto overflowing_count = std::numeric_limits<std::size_t>::max() / sizeof(source) + 2;
    auto manager = make_manager();
    using Buffer = vsl::DeviceLocalBuffer<vsl::MemoryType::TransferSource>;
    EXPECT_THROW((Buffer(device, manager, &source, overflowing_count)), vsl::exceptions::RuntimeException);
}
TEST_F(BufferVulkanIntegrationTest, ArrayConstructionRejectsZeroElementsBeforeCreatingNativeBuffers) {
    const std::uint32_t source = 29;
    EXPECT_THROW((HostBuffer(device, &source, std::size_t{0})), vsl::exceptions::RuntimeException);
    auto manager = make_manager();
    using Buffer = vsl::DeviceLocalBuffer<vsl::MemoryType::TransferSource>;
    EXPECT_THROW((Buffer(device, manager, &source, std::size_t{0})), vsl::exceptions::RuntimeException);
}
} // namespace
