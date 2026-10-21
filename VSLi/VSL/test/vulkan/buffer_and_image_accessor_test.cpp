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
        EXPECT_TRUE(contain(ImageType::TRANSFER_SRC | ImageType::SAMPLED, ImageType::TRANSFER_SRC));
        EXPECT_FALSE(contain(MemoryType::UNIFORM_BUFFER, MemoryType::STORAGE_BUFFER));
        auto properties = MemoryProperty::HOST_VISIBLE;
        properties |= MemoryProperty::HOST_COHERENT;
        EXPECT_TRUE(contain(properties, MemoryProperty::HOST_VISIBLE | MemoryProperty::HOST_COHERENT));
        EXPECT_EQ(properties & MemoryProperty::DEVICE_LOCAL, MemoryProperty::NONE);
    }
} // namespace
#include "support/image_transfer_fixture.hpp"
namespace {
    using ImageAccessorVulkanIntegrationTest = vsl_test::ImageTransferVulkanIntegrationTest;
    TEST_F(ImageAccessorVulkanIntegrationTest, AccessorTransfersAndMappedHolderReadBackPixels) {
        const std::array<std::uint32_t, 4> expected{127, 257, 65537, 0xffffffff};
        TransferImage image(device, 2, 2, vsl::data_format::UNSIGNED_INT);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_SOURCE> upload(device, expected);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> readback(device, sizeof(expected));
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            transition(manager, image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral, {},
                       vk::AccessFlagBits::eTransferWrite);
        }
        device.data_->device.waitIdle();
        EXPECT_TRUE(static_cast<vsl::ImageAccessor&>(image).copy_by_buffer(manager, &upload,
                                                                         vsl::ImageLayout::GENERAL, 0));
        device.data_->device.waitIdle();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            transition(manager, image, vk::ImageLayout::eGeneral, vk::ImageLayout::eGeneral,
                       vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead);
        }
        device.data_->device.waitIdle();
        EXPECT_TRUE(readback.copy_by_image(manager, image));
        device.data_->device.waitIdle();
        {
            auto mapped = readback.data(sizeof(expected));
            EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
            EXPECT_TRUE(mapped.copy_by_image(manager, image));
            device.data_->device.waitIdle();
            EXPECT_EQ(std::memcmp(mapped.data, expected.data(), sizeof(expected)), 0);
        }
    }
} // namespace

namespace {
    static_assert(std::has_virtual_destructor_v<vsl::BufferAccessor>);
    TEST(BufferAccessorLifetimeTest, BaseOwnerRunsTheDerivedDestructor) {
        struct TrackedBuffer : vsl::BufferAccessor {
            bool& destroyed;
            explicit TrackedBuffer(bool& destroyed) : destroyed(destroyed) {}
            ~TrackedBuffer() { destroyed = true; }
            LocalBufferHolder data(std::optional<std::size_t>, std::size_t) override {
                return {nullptr, 0, 0};
            }
            void flush(LocalBufferHolder&) override {}
        };
        bool destroyed = false;
        std::unique_ptr<vsl::BufferAccessor> buffer = std::make_unique<TrackedBuffer>(destroyed);
        buffer.reset();
        EXPECT_TRUE(destroyed);
    }
    using BufferAccessorVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(BufferAccessorVulkanIntegrationTest, MovesTransferTheBufferAndCopiesShareItsContents) {
        using Buffer = vsl::StagingBuffer<vsl::MemoryType::STORAGE_BUFFER>;
        const std::array<std::uint32_t, 4> values{3, 7, 11, 19};
        Buffer original(device, values);
        const auto storage = original.data_;
        Buffer copied = original;
        EXPECT_EQ(copied.data_, storage);
        Buffer moved = std::move(original);
        EXPECT_FALSE(original.data_);
        EXPECT_EQ(moved.data_, storage);
        original = std::move(moved);
        EXPECT_FALSE(moved.data_);
        EXPECT_EQ(original.data_, storage);
        copied = original;
        auto mapped = copied.data(sizeof(values));
        EXPECT_EQ(std::memcmp(mapped.data, values.data(), sizeof(values)), 0);
    }
    TEST_F(BufferAccessorVulkanIntegrationTest, MappingKeepsStorageAliveAfterOriginalBufferIsDestroyed) {
        std::optional<Holder> mapping;
        std::weak_ptr<vsl::impl::BufferImpl> storage;
        const std::array<std::uint32_t, 2> expected{17, 43};
        {
            vsl::StagingBuffer<> buffer(device, expected);
            storage = buffer.data_;
            mapping.emplace(buffer.data());
        }
        if (storage.expired())
            mapping->parent = nullptr;
        ASSERT_FALSE(storage.expired());
        EXPECT_EQ(std::memcmp(mapping->data, expected.data(), sizeof(expected)), 0);
        mapping->flush();
        mapping.reset();
        EXPECT_TRUE(storage.expired());
    }
} // namespace
