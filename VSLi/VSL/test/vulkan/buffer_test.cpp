#include "support/vulkan_fixture.hpp"
#include <array>

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
TEST_F(BufferVulkanIntegrationTest, DISABLED_VariadicCopyStoresValuesConsecutively) {
    HostBuffer buffer(device, std::size_t{8});
    const std::uint32_t first = 17, second = 29;
    ASSERT_TRUE(buffer.copy(first, second));
    auto mapped = buffer.data(8);
    std::array<std::uint32_t, 2> actual{};
    std::memcpy(actual.data(), mapped.data, sizeof(actual));
    EXPECT_EQ(actual, (std::array<std::uint32_t, 2>{first, second}));
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
#include "support/compile_probe.hpp"
namespace {
TEST(BufferCompilationTest, DISABLED_HostVisibleNonCoherentValueConstructorCanFlush) {
    const auto result = vsl_test::compile_probe(R"cpp(
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vsl.hpp>
void probe(vsl::LogicalDeviceAccessor device){
    vsl::Buffer<vsl::MemoryType::UniformBuffer,vsl::MemoryProperty::HostVisible> buffer(device,1.0f);
}
)cpp");
    EXPECT_EQ(result.status, 0) << result.diagnostics;
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
