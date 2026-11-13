#include <VSL/lifetime/startup_access.hpp>
#include "support/vulkan_fixture.hpp"

namespace {
    using VSL_NAMESPACE::lifetime::detail::StartupAccess;
}

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
        const auto expected = StartupAccess::Owned(physical)->device.getProperties();
        StartupAccess::Owned(physical)->props.reset();
        EXPECT_EQ(physical.api_version(), expected.apiVersion);
        EXPECT_EQ(physical.driver_version(), expected.driverVersion);
        EXPECT_EQ(physical.vendor_id(), expected.vendorID);
        EXPECT_EQ(physical.device_id(), expected.deviceID);
        const std::string expected_name = expected.deviceName;
        EXPECT_EQ(physical.name(), expected_name);
        auto type = physical.type();
        EXPECT_EQ(type.index(), static_cast<char>(expected.deviceType));
        EXPECT_TRUE(StartupAccess::Owned(physical)->props.has_value());
    }
    TEST_F(DeviceVulkanIntegrationTest, EnumeratesDevicesFromAnExistingVulkanAccessor) {
        vsl::PhysicalDevices<vsl::PhysicalDevicesSercher> devices(vsl::lifetime::detail::StartupAccess::FromShared(instance));
        const auto found = devices.get();
        ASSERT_FALSE(found.empty());
        EXPECT_TRUE(std::ranges::any_of(
            found, [&](const auto& item) { return StartupAccess::Owned(item)->device == StartupAccess::Owned(physical)->device; }));
        for (auto item : found)
            EXPECT_FALSE(item.name().empty());
    }
} // namespace
#include "support/window_fixture.hpp"
namespace {
    using LogicalDeviceWindowIntegrationTest = vsl_test::WindowIntegrationTest;
    TEST_F(LogicalDeviceWindowIntegrationTest, ForeignInstanceSurfaceIsRejectedBeforeLogicalDeviceCreation) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        vsl::InstanceCreateInfo info;
        info.application_name = "foreign physical Instance";
        info.api_version = VK_API_VERSION_1_0;
        for (const auto& extension : vk::enumerateInstanceExtensionProperties())
            if (std::strcmp(extension.extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0)
                info.extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        auto other = vsl::VulkanAccessor::Create(std::move(info));
        auto selected = vsl::PhysicalDevice::Select(other);
        using Device = vsl::LogicalDevice<vsl::LogicalDeviceGraphicCreator<false>>;
        EXPECT_THROW((Device(selected, *surface)), std::invalid_argument);
    }
    TEST_F(LogicalDeviceWindowIntegrationTest, BothSurfaceOverloadsCreateUsableQueuesAndRetainPhysicalOwner) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        const auto features = StartupAccess::Owned(physical)->device.getFeatures();
        if (!features.independentBlend || !features.samplerAnisotropy ||
            (vsl::VALIDATION && (!features.vertexPipelineStoresAndAtomics || !features.fragmentStoresAndAtomics)))
            GTEST_SKIP() << "Public logical-device constructor requires unsupported fixed features";
#ifndef _MSC_VER
        const auto extensions = StartupAccess::Owned(physical)->device.enumerateDeviceExtensionProperties();
        if (!std::ranges::any_of(extensions, [](const auto& extension) {
                return std::strcmp(extension.extensionName, "VK_KHR_portability_subset") == 0;
            }))
            GTEST_SKIP()
                << "Public logical-device constructor requires VK_KHR_portability_subset on non-MSVC platforms";
#endif
        using Device = vsl::LogicalDevice<vsl::LogicalDeviceGraphicCreator<false>>;
        Device by_pointer(physical, surface), by_value(physical, *surface);
        EXPECT_EQ(StartupAccess::Owned(by_pointer.get_physical_device()), StartupAccess::Owned(physical));
        EXPECT_EQ(StartupAccess::Owned(by_value.get_physical_device()), StartupAccess::Owned(physical));
        EXPECT_TRUE(static_cast<bool>(by_pointer.data_->device.getQueue(by_pointer.data_->graphics_family, 0)));
        EXPECT_TRUE(static_cast<bool>(by_value.data_->device.getQueue(by_value.data_->present_family, 0)));
        by_pointer.data_->device.waitIdle();
        by_value.data_->device.waitIdle();
    }
    TEST_F(DeviceVulkanIntegrationTest, SearchReturnsAnEnumeratedDeviceAndBenchmarkMatchesHardwareClass) {
        vsl::PhysicalDevices<vsl::PhysicalDevicesSercher> candidates(vsl::lifetime::detail::StartupAccess::FromShared(instance));
        auto selected = candidates.search();
        const auto found = candidates.get();
        EXPECT_TRUE(std::ranges::any_of(
            found, [&](const auto& candidate) { return StartupAccess::Owned(candidate)->device == StartupAccess::Owned(selected)->device; }));
        const auto properties = StartupAccess::Owned(physical)->device.getProperties();
        const auto features = StartupAccess::Owned(physical)->device.getFeatures();
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
            RECEIVED_PHYSICAL = StartupAccess::Shared(physical);
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
        using Device = vsl::LogicalDevice<SuppliedLogicalDeviceCreator>;
        Device by_value(physical, *surface);
        EXPECT_EQ(by_value.data_, device.data_);
        EXPECT_EQ(StartupAccess::Owned(by_value.get_physical_device()), StartupAccess::Owned(physical));
        EXPECT_EQ(SuppliedLogicalDeviceCreator::RECEIVED_PHYSICAL, StartupAccess::Owned(physical));
        EXPECT_EQ(SuppliedLogicalDeviceCreator::RECEIVED_SURFACE, surface->data_);
        Device by_pointer(physical, surface);
        EXPECT_EQ(by_pointer.data_, device.data_);
        EXPECT_EQ(StartupAccess::Owned(by_pointer.get_physical_device()), StartupAccess::Owned(physical));
        EXPECT_TRUE(static_cast<bool>(by_pointer.data_->device.getQueue(queue_family, 0)));
    }
} // namespace

#include <functional>
namespace {
    struct ConfiguredDeviceBenchmark {
        static inline std::function<void()> CONSTRUCT;
        static inline std::function<int(vsl::PhysicalDevice)> EVALUATE;
        ConfiguredDeviceBenchmark() {
            const auto callback = CONSTRUCT;
            if (callback)
                callback();
        }
        int benchmark(vsl::PhysicalDevice device) { return EVALUATE(std::move(device)); }
    };
    struct ClearDeviceBenchmark {
        ~ClearDeviceBenchmark() {
            ConfiguredDeviceBenchmark::CONSTRUCT = {};
            ConfiguredDeviceBenchmark::EVALUATE = {};
        }
    };
    TEST_F(DeviceVulkanIntegrationTest, CustomSearcherKeepsTheHighestNonnegativeScoreAndFirstTie) {
        ClearDeviceBenchmark cleanup;
        vsl::PhysicalDevices<ConfiguredDeviceBenchmark> devices(vsl::lifetime::detail::StartupAccess::FromShared(instance));
        for (const auto& [scores, expected] : {
                 std::pair{std::array{90, 10, 2}, 0u}, std::pair{std::array{0, 12, 1}, 1u},
                 std::pair{std::array{1, 2, 99}, 2u}, std::pair{std::array{7, 7, 1}, 0u},
                 std::pair{std::array{0, 0, 0}, 0u}}) {
            StartupAccess::Collection(devices)->devices.assign(scores.size(), StartupAccess::Owned(physical)->device);
            const auto listed = devices.get();
            ASSERT_EQ(listed.size(), scores.size());
            std::vector<std::shared_ptr<vsl::impl::PhysicalDeviceImpl>> visited;
            ConfiguredDeviceBenchmark::EVALUATE = [&](vsl::PhysicalDevice candidate) {
                EXPECT_EQ(StartupAccess::Owned(candidate)->device, StartupAccess::Owned(physical)->device);
                const auto index = visited.size();
                visited.push_back(StartupAccess::Shared(candidate));
                if (index >= scores.size()) {
                    ADD_FAILURE() << "Unexpected extra candidate";
                    return -1;
                }
                return scores[index];
            };
            const auto selected = devices.search();
            ASSERT_EQ(visited.size(), scores.size());
            EXPECT_EQ(StartupAccess::Owned(selected), visited[expected]);
        }
    }
    TEST_F(DeviceVulkanIntegrationTest, CustomSearcherSurvivesSourceDestructionDuringItsConstructorAndBenchmark) {
        ClearDeviceBenchmark cleanup;
        using Devices = vsl::PhysicalDevices<ConfiguredDeviceBenchmark>;
        auto source = std::make_unique<Devices>(StartupAccess::FromShared(instance));
        StartupAccess::Collection(*source)->devices.assign(3, StartupAccess::Owned(physical)->device);
        const std::weak_ptr<vsl::impl::PhysicalDevicesImpl> state = StartupAccess::Collection(*source);
        ConfiguredDeviceBenchmark::CONSTRUCT = [&] {
            source.reset();
            EXPECT_FALSE(state.expired());
        };
        std::vector<vsl::PhysicalDevice> visited;
        ConfiguredDeviceBenchmark::EVALUATE = [&](vsl::PhysicalDevice candidate) {
            EXPECT_FALSE(source);
            visited.push_back(candidate);
            return static_cast<int>(visited.size());
        };
        const auto selected = source->search();
        EXPECT_FALSE(source);
        EXPECT_TRUE(state.expired());
        ASSERT_EQ(visited.size(), 3u);
        EXPECT_EQ(StartupAccess::Owned(selected), StartupAccess::Owned(visited.back()));
        EXPECT_FALSE(selected.get_instance().owner_ref().weak().expired());
    }
    TEST_F(DeviceVulkanIntegrationTest, CustomSearcherRejectsNegativeScoresAndEmptyEnumerations) {
        ClearDeviceBenchmark cleanup;
        vsl::PhysicalDevices<ConfiguredDeviceBenchmark> devices(vsl::lifetime::detail::StartupAccess::FromShared(instance));
        StartupAccess::Collection(devices)->devices.assign(2, StartupAccess::Owned(physical)->device);
        ConfiguredDeviceBenchmark::EVALUATE = [](vsl::PhysicalDevice) { return -1; };
        EXPECT_THROW(devices.search(), vsl::exceptions::RuntimeException);
        StartupAccess::Collection(devices)->devices.clear();
        ConfiguredDeviceBenchmark::EVALUATE = [](vsl::PhysicalDevice) {
            ADD_FAILURE() << "An empty device list must not be benchmarked";
            return 1;
        };
        EXPECT_TRUE(devices.get().empty());
        EXPECT_THROW(devices.search(), vsl::exceptions::RuntimeException);
    }
} // namespace
