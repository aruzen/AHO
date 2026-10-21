#include "support/vulkan_fixture.hpp"

namespace {
    using DeviceVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST(PhysicalDeviceTypeTest, NumericTypesMapToNamedCategoriesAndUnknownFallsBack) {
        using namespace vsl::physical_device_type;
        EXPECT_STREQ(get_type(0).name(), "OTHER");
        EXPECT_STREQ(get_type(1).name(), "INTEGRATED_GPU");
        EXPECT_STREQ(get_type(2).name(), "DISCRETE_GPU");
        EXPECT_STREQ(get_type(3).name(), "VIRTUAL_GPU");
        EXPECT_STREQ(get_type(4).name(), "CPU");
        EXPECT_STREQ(get_type(99).name(), "OTHER");
    }
    TEST_F(DeviceVulkanIntegrationTest, ReportsHardwarePropertiesAndLoadsEmptyCaches) {
        const auto expected = physical.data_->device.getProperties();
        physical.data_->props.reset();
        EXPECT_EQ(physical.api_version(), expected.apiVersion);
        EXPECT_EQ(physical.driver_version(), expected.driverVersion);
        EXPECT_EQ(physical.vendor_id(), expected.vendorID);
        EXPECT_EQ(physical.device_id(), expected.deviceID);
        EXPECT_EQ(physical.name(), std::string(expected.deviceName));
        auto type = physical.type();
        EXPECT_EQ(type.index(), static_cast<char>(expected.deviceType));
        EXPECT_TRUE(physical.data_->props.has_value());
    }
    TEST_F(DeviceVulkanIntegrationTest, EnumeratesDevicesFromAnExistingVulkanAccessor) {
        vsl::PhysicalDevices<vsl::PhysicalDevicesSercher> devices(vsl::VulkanAccessor{instance});
        const auto found = devices.get();
        ASSERT_FALSE(found.empty());
        EXPECT_TRUE(std::ranges::any_of(
            found, [&](const auto& item) { return item.data_->device == physical.data_->device; }));
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
        const auto features = physical.data_->device.getFeatures();
        if (!features.independentBlend || !features.samplerAnisotropy ||
            (vsl::VALIDATION && (!features.vertexPipelineStoresAndAtomics || !features.fragmentStoresAndAtomics)))
            GTEST_SKIP() << "Public logical-device constructor requires unsupported fixed features";
#ifndef _MSC_VER
        const auto extensions = physical.data_->device.enumerateDeviceExtensionProperties();
        if (!std::ranges::any_of(extensions, [](const auto& extension) {
                return std::strcmp(extension.extensionName, "VK_KHR_portability_subset") == 0;
            }))
            GTEST_SKIP()
                << "Public logical-device constructor requires VK_KHR_portability_subset on non-MSVC platforms";
#endif
        using Device = vsl::LogicalDevice<vsl::LogicalDeviceGraphicCreator<false>>;
        Device by_pointer(physical, surface), by_value(physical, *surface);
        EXPECT_EQ(by_pointer.data_->parent_device, physical.data_);
        EXPECT_EQ(by_value.data_->parent_device, physical.data_);
        EXPECT_TRUE(static_cast<bool>(by_pointer.data_->device.getQueue(by_pointer.data_->graphics_family, 0)));
        EXPECT_TRUE(static_cast<bool>(by_value.data_->device.getQueue(by_value.data_->present_family, 0)));
        by_pointer.data_->device.waitIdle();
        by_value.data_->device.waitIdle();
    }
    TEST_F(DeviceVulkanIntegrationTest, SearchReturnsAnEnumeratedDeviceAndBenchmarkMatchesHardwareClass) {
        vsl::PhysicalDevices<vsl::PhysicalDevicesSercher> candidates(vsl::VulkanAccessor{instance});
        auto selected = candidates.search();
        const auto found = candidates.get();
        EXPECT_TRUE(std::ranges::any_of(
            found, [&](const auto& candidate) { return candidate.data_->device == selected.data_->device; }));
        const auto properties = physical.data_->device.getProperties();
        const auto features = physical.data_->device.getFeatures();
        const int expected = features.geometryShader
                                 ? static_cast<int>(properties.limits.maxImageDimension2D) +
                                       (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu ? 1000 : 0)
                                 : 0;
        EXPECT_EQ(vsl::PhysicalDevicesSercher{}.benchmark(physical), expected);
        // Maximum-score selection requires a multi-device fixture; single-device enumeration cannot prove it.
    }
} // namespace

namespace {
    struct SuppliedLogicalDeviceCreator {
        static inline std::shared_ptr<vsl::impl::LogicalDeviceImpl> SUPPLIED;
        static inline std::shared_ptr<vsl::impl::PhysicalDeviceImpl> RECEIVED_PHYSICAL;
        static inline std::shared_ptr<vsl::impl::SurfaceImpl> RECEIVED_SURFACE;
        std::shared_ptr<vsl::impl::LogicalDeviceImpl> create(vsl::PhysicalDevice physical, vsl::Surface surface) {
            RECEIVED_PHYSICAL = physical.data_;
            RECEIVED_SURFACE = surface.data_;
            return SUPPLIED;
        }
    };
    TEST_F(LogicalDeviceWindowIntegrationTest, CustomCreatorLinksForBothSurfaceOverloadsAndPinsThePhysicalOwner) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        struct ResetCreator {
            ~ResetCreator() {
                SuppliedLogicalDeviceCreator::SUPPLIED.reset();
                SuppliedLogicalDeviceCreator::RECEIVED_PHYSICAL.reset();
                SuppliedLogicalDeviceCreator::RECEIVED_SURFACE.reset();
            }
        } cleanup;
        SuppliedLogicalDeviceCreator::SUPPLIED = device.data_;
        device.data_->parent_device.reset();
        using Device = vsl::LogicalDevice<SuppliedLogicalDeviceCreator>;
        Device by_value(physical, *surface);
        EXPECT_EQ(by_value.data_, device.data_);
        EXPECT_EQ(by_value.data_->parent_device, physical.data_);
        EXPECT_EQ(SuppliedLogicalDeviceCreator::RECEIVED_PHYSICAL, physical.data_);
        EXPECT_EQ(SuppliedLogicalDeviceCreator::RECEIVED_SURFACE, surface->data_);
        device.data_->parent_device.reset();
        Device by_pointer(physical, surface);
        EXPECT_EQ(by_pointer.data_, device.data_);
        EXPECT_EQ(by_pointer.data_->parent_device, physical.data_);
        EXPECT_TRUE(static_cast<bool>(by_pointer.data_->device.getQueue(queue_family, 0)));
    }
} // namespace

#include <functional>
namespace {
    struct ConfiguredDeviceBenchmark {
        static inline std::function<int(vsl::PhysicalDevice)> EVALUATE;
        int benchmark(vsl::PhysicalDevice device) { return EVALUATE(std::move(device)); }
    };
    struct ClearDeviceBenchmark {
        ~ClearDeviceBenchmark() { ConfiguredDeviceBenchmark::EVALUATE = {}; }
    };
    TEST_F(DeviceVulkanIntegrationTest, CustomSearcherKeepsTheHighestNonnegativeScoreAndFirstTie) {
        ClearDeviceBenchmark cleanup;
        vsl::PhysicalDevices<ConfiguredDeviceBenchmark> devices(vsl::VulkanAccessor{instance});
        for (const auto& [scores, expected] : {
                 std::pair{std::array{90, 10, 2}, 0u}, std::pair{std::array{0, 12, 1}, 1u},
                 std::pair{std::array{1, 2, 99}, 2u}, std::pair{std::array{7, 7, 1}, 0u},
                 std::pair{std::array{0, 0, 0}, 0u}}) {
            devices.data_->devices.assign(scores.size(), physical.data_->device);
            const auto listed = devices.get();
            ASSERT_EQ(listed.size(), scores.size());
            std::vector<std::shared_ptr<vsl::impl::PhysicalDeviceImpl>> visited;
            ConfiguredDeviceBenchmark::EVALUATE = [&](vsl::PhysicalDevice candidate) {
                EXPECT_EQ(candidate.data_->device, physical.data_->device);
                const auto index = visited.size();
                visited.push_back(candidate.data_);
                if (index >= scores.size()) {
                    ADD_FAILURE() << "Unexpected extra candidate";
                    return -1;
                }
                return scores[index];
            };
            const auto selected = devices.search();
            ASSERT_EQ(visited.size(), scores.size());
            EXPECT_EQ(selected.data_, visited[expected]);
        }
    }
    TEST_F(DeviceVulkanIntegrationTest, CustomSearcherRejectsNegativeScoresAndEmptyEnumerations) {
        ClearDeviceBenchmark cleanup;
        vsl::PhysicalDevices<ConfiguredDeviceBenchmark> devices(vsl::VulkanAccessor{instance});
        devices.data_->devices.assign(2, physical.data_->device);
        ConfiguredDeviceBenchmark::EVALUATE = [](vsl::PhysicalDevice) { return -1; };
        EXPECT_THROW(devices.search(), vsl::exceptions::RuntimeException);
        devices.data_->devices.clear();
        ConfiguredDeviceBenchmark::EVALUATE = [](vsl::PhysicalDevice) {
            ADD_FAILURE() << "An empty device list must not be benchmarked";
            return 1;
        };
        EXPECT_TRUE(devices.get().empty());
        EXPECT_THROW(devices.search(), vsl::exceptions::RuntimeException);
    }
} // namespace
