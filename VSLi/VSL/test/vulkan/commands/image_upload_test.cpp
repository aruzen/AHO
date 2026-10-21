#include "support/vulkan_fixture.hpp"
#include <VSL/vulkan/commands/image_upload.hpp>
#include <VSL/vulkan/commands/image_readback.hpp>
#include <optional>
#include <array>

namespace {
    class ImageUploadVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    bool ImageUploadVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }

    TEST_F(ImageUploadVulkanIntegrationTest, OwnsMappedSourceAfterTheCallerDropsItAndCopiesAnOffsetRegion) {
        using Image = vsl::Image<vsl::ImageType::Sampled | vsl::ImageType::TransferDst | vsl::ImageType::TransferSrc>;
        auto manager = make_manager();
        std::optional<Image> image(std::in_place, device, 2, 2, vsl::data_format::UnsignedNormalized8RGBA);
        vsl::Buffer<vsl::MemoryType::TransferDestination, vsl::MemoryProperty::HostVisible> destination(device, std::size_t{16});
        auto destinationMapping = destination.data();
        std::weak_ptr<vsl::_impl::Buffer_impl> weak_source;
        std::weak_ptr<vsl::_impl::Image_impl> weak_image = image->_data;
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            {
                vsl::Buffer<vsl::MemoryType::TransferSource, vsl::MemoryProperty::HostVisible> source(device, std::size_t{8});
                const std::array<std::uint8_t, 8> data{0, 0, 0, 0, 19, 37, 73, 255};

                source.data().write(std::as_bytes(std::span(data)));
                weak_source = source._data;
                phase << vsl::command::ImageUpload(&source, *image, vsl::ImageLayout::Undefined,
                                                   {.x = 1, .y = 1, .width = 1, .height = 1}, 4);
            }
            EXPECT_FALSE(weak_source.expired());
            phase << vsl::command::ImageReadback(&destination, *image, vsl::ImageLayout::ShaderReadOnlyOptimal);
            image.reset();
            EXPECT_FALSE(weak_image.expired());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::Success);
            ASSERT_TRUE(phase.completion().wait());
        }

        const auto bytes = destinationMapping.read(16);
        for (std::size_t i = 0; i < 12; ++i)
            EXPECT_EQ(bytes[i], std::byte{});
        EXPECT_EQ(bytes[12], std::byte{19});
        EXPECT_EQ(bytes[13], std::byte{37});
        EXPECT_EQ(bytes[14], std::byte{73});
        EXPECT_EQ(bytes[15], std::byte{255});
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase.submit();
            ASSERT_TRUE(phase.completion().wait());
        }
        EXPECT_TRUE(weak_source.expired());
        EXPECT_TRUE(weak_image.expired());
    }

    TEST_F(ImageUploadVulkanIntegrationTest, RejectsInvalidUsageLayoutAndRangesBeforeRecording) {
        using Image = vsl::Image<vsl::ImageType::Sampled | vsl::ImageType::TransferDst>;
        Image image(device, 2, 2, vsl::data_format::UnsignedNormalized8RGBA);
        vsl::Buffer<vsl::MemoryType::TransferSource, vsl::MemoryProperty::HostVisible> source(device, std::size_t{16});
        vsl::Buffer<vsl::MemoryType::TransferDestination, vsl::MemoryProperty::HostVisible> wrong(device, std::size_t{16});
        EXPECT_THROW((vsl::command::ImageUpload(&wrong, image, vsl::ImageLayout::Undefined).copy.description()), std::invalid_argument);
        auto manager = make_manager();
        auto phase = manager.startPhase<vsl::ComputePhase>();
        EXPECT_THROW(phase << vsl::command::ImageUpload(&source, image, vsl::ImageLayout::General), std::invalid_argument);
        EXPECT_THROW(phase << vsl::command::ImageUpload(&source, image, vsl::ImageLayout::Undefined, {.x = 2, .width = 1, .height = 1}), std::out_of_range);
        EXPECT_THROW(phase << vsl::command::ImageUpload(&source, image, vsl::ImageLayout::Undefined, {}, 4), std::out_of_range);
        EXPECT_THROW(phase << vsl::command::ImageUpload(&source, image, vsl::ImageLayout::Undefined, {.width = 1, .height = 1}, 1), std::invalid_argument);
        phase.cancel();
    }
} // namespace
