#include "support/vulkan_fixture.hpp"

namespace {
using DeviceVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
TEST(PhysicalDeviceTypeTest, NumericTypesMapToNamedCategoriesAndUnknownFallsBack) {
    using namespace vsl::PhysicalDeviceType;
    EXPECT_STREQ(getType(0).name(), "OTHER");
    EXPECT_STREQ(getType(1).name(), "INTEGRATED_GPU");
    EXPECT_STREQ(getType(2).name(), "DISCRETE_GPU");
    EXPECT_STREQ(getType(3).name(), "VIRTUAL_GPU");
    EXPECT_STREQ(getType(4).name(), "CPU");
    EXPECT_STREQ(getType(99).name(), "OTHER");
}
TEST_F(DeviceVulkanIntegrationTest, ReportsHardwarePropertiesAndLoadsEmptyCaches) {
    const auto expected = physical._data->device.getProperties();
    physical._data->props.reset();
    EXPECT_EQ(physical.apiVersion(), expected.apiVersion);
    EXPECT_EQ(physical.driverVersion(), expected.driverVersion);
    EXPECT_EQ(physical.vendorID(), expected.vendorID);
    EXPECT_EQ(physical.deviceID(), expected.deviceID);
    EXPECT_EQ(physical.name(), std::string(expected.deviceName));
    auto type = physical.type();
    EXPECT_EQ(type.index(), static_cast<char>(expected.deviceType));
    EXPECT_TRUE(physical._data->props.has_value());
}
TEST_F(DeviceVulkanIntegrationTest, EnumeratesDevicesFromAnExistingVulkanAccessor) {
    vsl::PhysicalDevices<vsl::_PhysicalDevicesSercher> devices(vsl::VulkanAccessor{instance});
    const auto found = devices.get();
    ASSERT_FALSE(found.empty());
    EXPECT_TRUE(std::ranges::any_of(
        found, [&](const auto &item) { return item._data->device == physical._data->device; }));
    for (auto item : found)
        EXPECT_FALSE(item.name().empty());
}
} // namespace
#include "support/window_fixture.hpp"
namespace {
using LogicalDeviceWindowIntegrationTest = vsl_test::WindowIntegrationTest;
TEST_F(LogicalDeviceWindowIntegrationTest, BothSurfaceOverloadsCreateUsableQueuesAndRetainPhysicalOwner) {
    make_surface();
    ASSERT_FALSE(HasFatalFailure());
    const auto features = physical._data->device.getFeatures();
    if (!features.independentBlend || !features.samplerAnisotropy ||
        (vsl::validation && (!features.vertexPipelineStoresAndAtomics || !features.fragmentStoresAndAtomics)))
        GTEST_SKIP() << "Public logical-device constructor requires unsupported fixed features";
#ifndef _MSC_VER
    const auto extensions = physical._data->device.enumerateDeviceExtensionProperties();
    if (!std::ranges::any_of(extensions, [](const auto &extension) {
            return std::strcmp(extension.extensionName, "VK_KHR_portability_subset") == 0;
        }))
        GTEST_SKIP()
            << "Public logical-device constructor requires VK_KHR_portability_subset on non-MSVC platforms";
#endif
    using Device = vsl::LogicalDevice<vsl::_LogicalDeviceGraphicCreator<false>>;
    Device by_pointer(physical, surface), by_value(physical, *surface);
    EXPECT_EQ(by_pointer._data->parentDevice, physical._data);
    EXPECT_EQ(by_value._data->parentDevice, physical._data);
    EXPECT_TRUE(static_cast<bool>(by_pointer._data->device.getQueue(by_pointer._data->graphicsFamily, 0)));
    EXPECT_TRUE(static_cast<bool>(by_value._data->device.getQueue(by_value._data->presentFamily, 0)));
    by_pointer._data->device.waitIdle();
    by_value._data->device.waitIdle();
}
TEST_F(DeviceVulkanIntegrationTest, SearchReturnsAnEnumeratedDeviceAndBenchmarkMatchesHardwareClass) {
    vsl::PhysicalDevices<vsl::_PhysicalDevicesSercher> candidates(vsl::VulkanAccessor{instance});
    auto selected = candidates.search();
    const auto found = candidates.get();
    EXPECT_TRUE(std::ranges::any_of(
        found, [&](const auto &candidate) { return candidate._data->device == selected._data->device; }));
    const auto properties = physical._data->device.getProperties();
    const auto features = physical._data->device.getFeatures();
    const int expected = features.geometryShader
                             ? static_cast<int>(properties.limits.maxImageDimension2D) +
                                   (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu ? 1000 : 0)
                             : 0;
    EXPECT_EQ(vsl::_PhysicalDevicesSercher{}.benchmark(physical), expected);
    // Maximum-score selection requires a multi-device fixture; single-device enumeration cannot prove it.
}
} // namespace

#include <functional>
namespace {
struct ConfiguredDeviceBenchmark {
    static inline std::function<int(vsl::PhysicalDevice)> evaluate;
    int benchmark(vsl::PhysicalDevice device) { return evaluate(std::move(device)); }
};
struct ClearDeviceBenchmark {
    ~ClearDeviceBenchmark() { ConfiguredDeviceBenchmark::evaluate = {}; }
};
TEST_F(DeviceVulkanIntegrationTest, CustomSearcherKeepsTheHighestNonnegativeScoreAndFirstTie) {
    ClearDeviceBenchmark cleanup;
    vsl::PhysicalDevices<ConfiguredDeviceBenchmark> devices(vsl::VulkanAccessor{instance});
    for (const auto &[scores, expected] : {
             std::pair{std::array{90, 10, 2}, 0u}, std::pair{std::array{0, 12, 1}, 1u},
             std::pair{std::array{1, 2, 99}, 2u}, std::pair{std::array{7, 7, 1}, 0u},
             std::pair{std::array{0, 0, 0}, 0u}}) {
        devices._data->devices.assign(scores.size(), physical._data->device);
        const auto listed = devices.get();
        ASSERT_EQ(listed.size(), scores.size());
        std::vector<std::shared_ptr<vsl::_impl::PhysicalDevice_impl>> visited;
        ConfiguredDeviceBenchmark::evaluate = [&](vsl::PhysicalDevice candidate) {
            EXPECT_EQ(candidate._data->device, physical._data->device);
            const auto index = visited.size();
            visited.push_back(candidate._data);
            if (index >= scores.size()) { ADD_FAILURE() << "Unexpected extra candidate"; return -1; }
            return scores[index];
        };
        const auto selected = devices.search();
        ASSERT_EQ(visited.size(), scores.size());
        EXPECT_EQ(selected._data, visited[expected]);
    }
}
TEST_F(DeviceVulkanIntegrationTest, CustomSearcherRejectsNegativeScoresAndEmptyEnumerations) {
    ClearDeviceBenchmark cleanup;
    vsl::PhysicalDevices<ConfiguredDeviceBenchmark> devices(vsl::VulkanAccessor{instance});
    devices._data->devices.assign(2, physical._data->device);
    ConfiguredDeviceBenchmark::evaluate = [](vsl::PhysicalDevice) { return -1; };
    EXPECT_THROW(devices.search(), vsl::exceptions::RuntimeException);
    devices._data->devices.clear();
    ConfiguredDeviceBenchmark::evaluate = [](vsl::PhysicalDevice) {
        ADD_FAILURE() << "An empty device list must not be benchmarked";
        return 1;
    };
    EXPECT_TRUE(devices.get().empty());
    EXPECT_THROW(devices.search(), vsl::exceptions::RuntimeException);
}
} // namespace
