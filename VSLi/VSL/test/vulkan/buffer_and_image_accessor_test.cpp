#include "support/vulkan_fixture.hpp"
#include <type_traits>

namespace {
    using Holder = vsl::BufferAccessor::LocalBufferHolder;
    static_assert(!std::is_copy_constructible_v<Holder> && !std::is_copy_assignable_v<Holder>);
    static_assert(std::is_move_constructible_v<Holder>);
    TEST(BufferHolderTest, MoveTransfersMappingAndDisarmsSource) {
        std::uint32_t value = 7;
        Holder source(nullptr, sizeof(value), 12, &value);
        Holder moved(std::move(source));
        EXPECT_EQ(moved.data, &value);
        EXPECT_EQ(moved.size, sizeof(value));
        EXPECT_EQ(moved.offset, 12u);
        EXPECT_EQ(source.parent, nullptr);
        EXPECT_EQ(source.data, nullptr);
        EXPECT_EQ(source.size, 0u);
        EXPECT_EQ(source.offset, 0u);
    }
    TEST(ResourceFlagsTest, UsageAndPropertiesSupportCombinedRequirements) {
        using namespace vsl;
        EXPECT_TRUE(contain(ImageType::TransferSrc | ImageType::Sampled, ImageType::TransferSrc));
        EXPECT_FALSE(contain(MemoryType::UniformBuffer, MemoryType::StorageBuffer));
        auto properties = MemoryProperty::HostVisible;
        properties |= MemoryProperty::HostCoherent;
        EXPECT_TRUE(contain(properties, MemoryProperty::HostVisible | MemoryProperty::HostCoherent));
        EXPECT_EQ(properties & MemoryProperty::DeviceLocal, MemoryProperty::None);
    }
} // namespace
#include "support/image_transfer_fixture.hpp"
namespace {
    using ImageAccessorVulkanIntegrationTest = vsl_test::ImageTransferVulkanIntegrationTest;
    TEST_F(ImageAccessorVulkanIntegrationTest, AccessorTransfersAndMappedHolderReadBackPixels) {
        const std::array<std::uint32_t, 4> expected{127, 257, 65537, 0xffffffff};
        TransferImage image(device, 2, 2, vsl::data_format::UnsignedInt);
        vsl::StagingBuffer<vsl::MemoryType::TransferSource> upload(device, expected);
        vsl::StagingBuffer<vsl::MemoryType::TransferDestination> readback(device, sizeof(expected));
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            transition(manager, image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral, {},
                       vk::AccessFlagBits::eTransferWrite);
        }
        device._data->device.waitIdle();
        EXPECT_TRUE(static_cast<vsl::ImageAccessor &>(image).copyByBuffer(manager, &upload,
                                                                          vsl::ImageLayout::General, 0));
        device._data->device.waitIdle();
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            transition(manager, image, vk::ImageLayout::eGeneral, vk::ImageLayout::eGeneral,
                       vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead);
        }
        device._data->device.waitIdle();
        EXPECT_TRUE(readback.copyByImage(manager, image));
        device._data->device.waitIdle();
        {
            auto mapped = readback.data(sizeof(expected));
            EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
            EXPECT_TRUE(mapped.copyByImage(manager, image));
            device._data->device.waitIdle();
            EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
        }
    }
} // namespace

namespace {
    static_assert(std::has_virtual_destructor_v<vsl::BufferAccessor>);
    TEST(BufferAccessorLifetimeTest, BaseOwnerRunsTheDerivedDestructor) {
        struct TrackedBuffer : vsl::BufferAccessor {
            bool &destroyed;
            explicit TrackedBuffer(bool &destroyed) : destroyed(destroyed) {}
            ~TrackedBuffer() { destroyed = true; }
            LocalBufferHolder data(std::optional<std::size_t>, std::size_t) override {
                return {nullptr, 0, 0};
            }
            void flush(LocalBufferHolder &) override {}
        };
        bool destroyed = false;
        std::unique_ptr<vsl::BufferAccessor> buffer = std::make_unique<TrackedBuffer>(destroyed);
        buffer.reset();
        EXPECT_TRUE(destroyed);
    }
    using BufferAccessorVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(BufferAccessorVulkanIntegrationTest, MovesTransferTheBufferAndCopiesShareItsContents) {
        using Buffer = vsl::StagingBuffer<vsl::MemoryType::StorageBuffer>;
        const std::array<std::uint32_t, 4> values{3, 7, 11, 19};
        Buffer original(device, values);
        const auto storage = original._data;
        Buffer copied = original;
        EXPECT_EQ(copied._data, storage);
        Buffer moved = std::move(original);
        EXPECT_FALSE(original._data);
        EXPECT_EQ(moved._data, storage);
        original = std::move(moved);
        EXPECT_FALSE(moved._data);
        EXPECT_EQ(original._data, storage);
        copied = original;
        auto mapped = copied.data(sizeof(values));
        EXPECT_EQ(std::memcmp(mapped.data, values.data(), sizeof(values)), 0);
    }
} // namespace
