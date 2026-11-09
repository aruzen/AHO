#include "support/vulkan_fixture.hpp"
#include <VSL/vulkan/commands/image_upload.hpp>
#include <VSL/vulkan/commands/image_readback.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>
#include <VSL/lifetime/buffer_arena.hpp>
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
        using Image = vsl::Image<vsl::ImageType::SAMPLED | vsl::ImageType::TRANSFER_DST | vsl::ImageType::TRANSFER_SRC>;
        auto manager = make_manager();
        std::optional<Image> image(std::in_place, device, 2, 2, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
        vsl::Buffer<vsl::MemoryType::TRANSFER_DESTINATION, vsl::MemoryProperty::HOST_VISIBLE> destination(device, std::size_t{16});
        auto destination_mapping = destination.data();
        std::weak_ptr<vsl::impl::BufferImpl> weak_source;
        std::weak_ptr<vsl::impl::ImageImpl> weak_image = image->data_;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            {
                vsl::Buffer<vsl::MemoryType::TRANSFER_SOURCE, vsl::MemoryProperty::HOST_VISIBLE> source(device, std::size_t{8});
                const std::array<std::uint8_t, 8> data{0, 0, 0, 0, 19, 37, 73, 255};

                source.data().write(std::as_bytes(std::span(data)));
                weak_source = source.data_;
                phase << vsl::command::ImageUpload(&source, *image, vsl::ImageLayout::UNDEFINED,
                                                   {.x = 1, .y = 1, .width = 1, .height = 1}, 4);
            }
            EXPECT_FALSE(weak_source.expired());
            phase << vsl::command::ImageReadback(&destination, *image, vsl::ImageLayout::SHADER_READ_ONLY_OPTIMAL);
            image.reset();
            EXPECT_FALSE(weak_image.expired());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }

        const auto bytes = destination_mapping.read(16);
        for (std::size_t i = 0; i < 12; ++i)
            EXPECT_EQ(bytes[i], std::byte{});
        EXPECT_EQ(bytes[12], std::byte{19});
        EXPECT_EQ(bytes[13], std::byte{37});
        EXPECT_EQ(bytes[14], std::byte{73});
        EXPECT_EQ(bytes[15], std::byte{255});
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase.submit();
            ASSERT_TRUE(phase.completion().wait());
        }
        EXPECT_TRUE(weak_source.expired());
        EXPECT_TRUE(weak_image.expired());
    }

    TEST_F(ImageUploadVulkanIntegrationTest, RejectsInvalidUsageLayoutAndRangesBeforeRecording) {
        using Image = vsl::Image<vsl::ImageType::SAMPLED | vsl::ImageType::TRANSFER_DST>;
        Image image(device, 2, 2, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
        vsl::Buffer<vsl::MemoryType::TRANSFER_SOURCE, vsl::MemoryProperty::HOST_VISIBLE> source(device, std::size_t{16});
        vsl::Buffer<vsl::MemoryType::TRANSFER_DESTINATION, vsl::MemoryProperty::HOST_VISIBLE> wrong(device, std::size_t{16});
        EXPECT_THROW((vsl::command::ImageUpload(&wrong, image, vsl::ImageLayout::UNDEFINED).copy.description()), std::invalid_argument);
        auto manager = make_manager();
        vsl::command::ImageUpload inactive(vsl::BufferView::FromBuffer(source), image, vsl::ImageLayout::UNDEFINED);
        EXPECT_THROW(inactive.invoke(manager.get_pool(), manager.get_buffer(), manager), std::logic_error);
        auto phase = manager.start_phase<vsl::ComputePhase>();
        EXPECT_THROW(phase << vsl::command::ImageUpload(&source, image, vsl::ImageLayout::GENERAL), std::invalid_argument);
        EXPECT_THROW(phase << vsl::command::ImageUpload(&source, image, vsl::ImageLayout::UNDEFINED, {.x = 2, .width = 1, .height = 1}), std::out_of_range);
        EXPECT_THROW(phase << vsl::command::ImageUpload(&source, image, vsl::ImageLayout::UNDEFINED, {}, 4), std::out_of_range);
        EXPECT_THROW(phase << vsl::command::ImageUpload(&source, image, vsl::ImageLayout::UNDEFINED, {.width = 1, .height = 1}, 1), std::invalid_argument);
        const auto whole = vsl::BufferView::FromBuffer(source);
        const auto selected = whole.view(4, 4);
        EXPECT_THROW((vsl::command::ImageUpload(selected, image, vsl::ImageLayout::UNDEFINED).copy.description()), std::out_of_range);
        EXPECT_THROW((vsl::command::ImageUpload(selected, image, vsl::ImageLayout::UNDEFINED, {.width = 1, .height = 1}, SIZE_MAX).copy.description()), std::out_of_range);
        EXPECT_THROW((vsl::command::ImageUpload(whole.view(1, 8), image, vsl::ImageLayout::UNDEFINED, {.width = 1, .height = 1}).copy.description()), std::invalid_argument);
        EXPECT_NO_THROW((vsl::command::ImageUpload(whole.view(1, 8), image, vsl::ImageLayout::UNDEFINED, {.width = 1, .height = 1}, 3).copy.description()));
        phase.cancel();
    }

    TEST_F(ImageUploadVulkanIntegrationTest, ViewUploadCapturesTheSelectedRangeAndRetainsSourceForItsPolicy) {
        using Image = vsl::Image<vsl::ImageType::SAMPLED | vsl::ImageType::TRANSFER_DST | vsl::ImageType::TRANSFER_SRC>;
        for (const auto retention : {vsl::ResourceRetention::COMPLETION, vsl::ResourceRetention::SUBMISSION}) {
            for (const bool cancel : {false, true}) {
                std::weak_ptr<void> owner;
                {
                    auto manager = make_manager().independent(1);
                    Image image(device, 1, 1, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
                    const std::array<std::byte, 4> color{std::byte{19}, std::byte{37}, std::byte{73}, std::byte{255}};
                    std::optional<vsl::command::ImageUpload> command;
                    {
                        const vsl::BufferCreateInfo info{24, vsl::MemoryType::TRANSFER_SOURCE};
                        vsl::lifetime::BufferArena arena(device, std::span(&info, 1));
                        owner = arena.owner();
                        const auto source = arena.view(0).view(4, 16);
                        source.write(color, 8);
                        command.emplace(source, image, vsl::ImageLayout::UNDEFINED, vsl::ImageRegion{}, 8, vsl::ResourceUse{}, retention);
                    }
                    auto phase = manager.start_phase<vsl::ComputePhase>();
                    phase << *command;
                    command.reset();
                    EXPECT_FALSE(owner.expired());
                    if (cancel)
                        phase.cancel();
                    else {
                        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                        ASSERT_TRUE(phase.completion().wait());
                    }
                    EXPECT_EQ(owner.expired(), retention == vsl::ResourceRetention::COMPLETION);
                    if (!cancel) {
                        vsl::Buffer<vsl::MemoryType::TRANSFER_DESTINATION, vsl::MemoryProperty::HOST_VISIBLE> destination(device, 4);
                        auto reader = manager.independent(1);
                        auto read = reader.start_phase<vsl::ComputePhase>();
                        read << vsl::command::ImageReadback(&destination, image, vsl::ImageLayout::SHADER_READ_ONLY_OPTIMAL);
                        ASSERT_EQ(read.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                        ASSERT_TRUE(read.completion().wait());
                        std::array<std::byte, 4> actual{};
                        destination.read_into(actual);
                        EXPECT_EQ(actual, color);
                    }
                }
                EXPECT_TRUE(owner.expired());
            }
        }
    }

    TEST_F(ImageUploadVulkanIntegrationTest, SourcePollingCanDestroyEitherCommandAndContinueOrCancelRecording) {
        for (const bool array : {false, true}) {
            for (const bool cancel : {false, true}) {
                using Image = vsl::Image<vsl::ImageType::SAMPLED | vsl::ImageType::TRANSFER_DST>;
                Image image(device, 1, 1, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
                vsl::Buffer<vsl::MemoryType::TRANSFER_SOURCE, vsl::MemoryProperty::HOST_VISIBLE> source(device, 4);
                auto manager = make_manager().independent(1), other = manager.independent(1);
                std::optional<vsl::ComputePhase> recording, peer;
                std::optional<vsl::command::ImageUpload> command;
                std::optional<vsl::command::CopyBufferToArrayImage> array_command;
                bool released = false;
                auto submitted = manager.start_phase<vsl::ComputePhase>();
                source.resource_use().record(manager.get_buffer());
                manager.get_buffer().retain_until_complete(std::shared_ptr<int>(new int, [&](int* value) {
                    delete value;
                    released = true;
                    command.reset();
                    array_command.reset();
                    if (cancel) {
                        recording->cancel();
                        peer.emplace(other);
                    }
                }));
                ASSERT_EQ(submitted.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                device.data_->device.waitIdle();
                recording.emplace(other);
                if (array && !cancel)
                    *recording << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::UNDEFINED, vsl::ImageLayout::TRANSFER_DST_OPTIMAL);
                if (array)
                    array_command.emplace(image, &source, vsl::ImageLayout::TRANSFER_DST_OPTIMAL, 0);
                else
                    command.emplace(vsl::BufferView::FromBuffer(source), image, vsl::ImageLayout::UNDEFINED);
                if (cancel)
                    EXPECT_THROW(array ? array_command->invoke(other.get_pool(), other.get_buffer(), other) : command->invoke(other.get_pool(), other.get_buffer(), other), std::logic_error);
                else {
                    EXPECT_NO_THROW(array ? array_command->invoke(other.get_pool(), other.get_buffer(), other) : command->invoke(other.get_pool(), other.get_buffer(), other));
                    ASSERT_EQ(recording->submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                    ASSERT_TRUE(recording->completion().wait());
                }
                EXPECT_TRUE(released);
                if (cancel) {
                    ASSERT_TRUE(peer);
                    EXPECT_NO_THROW(*peer << vsl::command::ImageUpload(vsl::BufferView::FromBuffer(source), image, vsl::ImageLayout::UNDEFINED));
                    peer->cancel();
                }
            }
        }
    }
} // namespace
