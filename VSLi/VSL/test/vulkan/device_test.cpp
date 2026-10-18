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

namespace {
struct SuppliedLogicalDeviceCreator {
    static inline std::shared_ptr<vsl::_impl::LogicalDevice_impl> supplied;
    static inline std::shared_ptr<vsl::_impl::PhysicalDevice_impl> received_physical;
    static inline std::shared_ptr<vsl::_impl::Surface_impl> received_surface;
    std::shared_ptr<vsl::_impl::LogicalDevice_impl> create(vsl::PhysicalDevice physical, vsl::Surface surface) {
        received_physical = physical._data;
        received_surface = surface._data;
        return supplied;
    }
};
TEST_F(LogicalDeviceWindowIntegrationTest, CustomCreatorLinksForBothSurfaceOverloadsAndPinsThePhysicalOwner) {
    make_surface();
    ASSERT_FALSE(HasFatalFailure());
    struct ResetCreator {
        ~ResetCreator() {
            SuppliedLogicalDeviceCreator::supplied.reset();
            SuppliedLogicalDeviceCreator::received_physical.reset();
            SuppliedLogicalDeviceCreator::received_surface.reset();
        }
    } cleanup;
    SuppliedLogicalDeviceCreator::supplied = device._data;
    device._data->parentDevice.reset();
    using Device = vsl::LogicalDevice<SuppliedLogicalDeviceCreator>;
    Device by_value(physical, *surface);
    EXPECT_EQ(by_value._data, device._data);
    EXPECT_EQ(by_value._data->parentDevice, physical._data);
    EXPECT_EQ(SuppliedLogicalDeviceCreator::received_physical, physical._data);
    EXPECT_EQ(SuppliedLogicalDeviceCreator::received_surface, surface->_data);
    device._data->parentDevice.reset();
    Device by_pointer(physical, surface);
    EXPECT_EQ(by_pointer._data, device._data);
    EXPECT_EQ(by_pointer._data->parentDevice, physical._data);
    EXPECT_TRUE(static_cast<bool>(by_pointer._data->device.getQueue(queue_family, 0)));
}
} // namespace
