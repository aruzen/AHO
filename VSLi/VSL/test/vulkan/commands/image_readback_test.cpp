#include <VSL/lifetime/buffer_access.hpp>
#include <VSL/lifetime/command_access.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include "support/image_transfer_fixture.hpp"
#include <VSL/vulkan/commands/image_readback.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>
#include <VSL/lifetime/buffer_arena.hpp>
#include "support/deferred_action.hpp"

namespace {
    using VSL_NAMESPACE::lifetime::detail::BufferAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace {
    class ImageReadbackCommandVulkanIntegrationTest : public vsl_test::ImageTransferVulkanIntegrationTest {
        bool validate_vulkan() const override;

    protected:
        TransferImage initialized_image(vsl::CommandManager manager);
    };

    bool ImageReadbackCommandVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }

    ImageReadbackCommandVulkanIntegrationTest::TransferImage ImageReadbackCommandVulkanIntegrationTest::initialized_image(vsl::CommandManager manager) {
        const std::array<std::uint32_t, 4> values{11, 22, 33, 44};
        TransferImage image(device, 2, 2, vsl::data_format::UNSIGNED_INT);
        vsl::StagingBuffer<> source(device, values);
        vsl::ComputePhase phase(manager);
        phase << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL)
              << vsl::command::CopyBufferToImage(image, &source, vsl::ImageLayout::TRANSFER_DST_OPTIMAL);
        if (phase.submit() != vsl::ComputePhase::SubmitResult::SUCCESS || !phase.completion().wait())
            throw std::runtime_error("Image setup submission failed");
        return image;
    }

    TEST_F(ImageReadbackCommandVulkanIntegrationTest, RecordsCopyAndRestoresLayoutWithoutSubmitting) {
        const std::array<std::uint32_t, 4> first{11, 22, 33, 44}, second{55, 66, 77, 88};
        TransferImage image(device, 2, 2, vsl::data_format::UNSIGNED_INT);
        vsl::StagingBuffer<> first_source(device, first), second_source(device, second);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> pixel(device, sizeof(std::uint32_t));
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> full(device, sizeof(second));
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL)
                  << vsl::command::CopyBufferToImage(image, &first_source, vsl::ImageLayout::TRANSFER_DST_OPTIMAL)
                  << vsl::command::ImageReadback(&pixel, image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL, {1, 0, 1, 1})
                  << vsl::command::CopyBufferToImage(image, &second_source, vsl::ImageLayout::TRANSFER_DST_OPTIMAL)
                  << vsl::command::ImageReadback(vsl::BufferView::FromBuffer(full), image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL, {}, {}, vsl::ResourceRetention::SUBMISSION);
            EXPECT_EQ(phase.state, vsl::ComputePhase::State::WAIT_SUBMIT);
        }
        device.data_->device.waitIdle();
        auto mapped_pixel = pixel.data(sizeof(std::uint32_t));
        std::uint32_t actual_pixel;
        std::memcpy(&actual_pixel, mapped_pixel.data, sizeof(actual_pixel));
        EXPECT_EQ(actual_pixel, first[1]);
        auto mapped_full = full.data(sizeof(second));
        std::array<std::uint32_t, 4> actual;
        std::memcpy(actual.data(), mapped_full.data, sizeof(actual));
        EXPECT_EQ(actual, second);
    }

    TEST_F(ImageReadbackCommandVulkanIntegrationTest, SelectedLayerRestorationLeavesOtherLayersUsable) {
        const std::array<std::uint32_t, 4> expected{11, 22, 33, 44};
        TransferImage image(device, 2, 2, vsl::data_format::UNSIGNED_INT, 2);
        vsl::StagingBuffer<> source(device, expected);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> pixel(device, sizeof(std::uint32_t));
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> full(device, sizeof(expected));
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL)
                  << vsl::command::CopyBufferToArrayImage(image, &source, vsl::ImageLayout::TRANSFER_DST_OPTIMAL, 0)
                  << vsl::command::CopyBufferToArrayImage(image, &source, vsl::ImageLayout::TRANSFER_DST_OPTIMAL, 1)
                  << vsl::command::ImageReadback(&pixel, image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL, {1, 1, 1, 1, 1})
                  << vsl::command::CopyBufferToArrayImage(image, &source, vsl::ImageLayout::TRANSFER_DST_OPTIMAL, 1)
                  << vsl::command::ImageReadback(&full, image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL, {0, 0, 2, 2, 0});
        }
        device.data_->device.waitIdle();
        auto mapped = pixel.data(sizeof(std::uint32_t));
        std::uint32_t actual;
        std::memcpy(&actual, mapped.data, sizeof(actual));
        EXPECT_EQ(actual, expected[3]);
    }

    TEST_F(ImageReadbackCommandVulkanIntegrationTest, DescriptionAndInvalidContractsDoNotRecordCommands) {
        TransferImage image(device, 2, 2, vsl::data_format::UNSIGNED_INT);
        const auto info = vsl::command::ImageReadback::Describe(image, vsl::ImageLayout::GENERAL);
        EXPECT_EQ(info.region.width, 2u);
        EXPECT_EQ(info.region.height, 2u);
        EXPECT_EQ(info.row_stride, 8u);
        EXPECT_EQ(info.byte_size, 16u);
        EXPECT_EQ(info.format, vsl::data_format::UNSIGNED_INT);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> small(device, 4);
        auto manager = make_manager();
        auto invoke = [&](vsl::ImageLayout layout, vsl::ImageRegion region = {}) {
            vsl::command::ImageReadback(&small, image, layout, region)
                .invoke(manager.get_pool(), manager.get_buffer(), manager);
        };
        EXPECT_THROW(invoke(vsl::ImageLayout::UNDEFINED), std::invalid_argument);
        EXPECT_THROW(invoke(vsl::ImageLayout::PREINITIALIZED), std::invalid_argument);
        EXPECT_THROW(invoke(vsl::ImageLayout::GENERAL), std::out_of_range);
        EXPECT_THROW(invoke(vsl::ImageLayout::GENERAL, {2, 0, 1, 1}), std::out_of_range);
    }
} // namespace

namespace {
    TEST_F(ImageReadbackCommandVulkanIntegrationTest, EmptyCommandBufferIsRejectedBeforeItIsDereferenced) {
        auto manager = make_manager();
        auto image = initialized_image(manager);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> destination(device, std::size_t{16});
        const vsl::CommandBuffer empty(CommandAccess::RawBuffer(std::shared_ptr<vsl::impl::CommandBufferImpl>{}));
        vsl::command::ImageReadback command(&destination, image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL);
        EXPECT_THROW(command.invoke(manager.get_pool(), empty, manager), std::invalid_argument);
    }
    TEST_F(ImageReadbackCommandVulkanIntegrationTest, EmptyManagerIsRejectedBeforeRetainingResourcesOrRecordingBarriers) {
        auto manager = make_manager();
        auto image = initialized_image(manager);
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> destination(device, std::size_t{16});
        const vsl::CommandManager empty(CommandAccess::RawManager(std::shared_ptr<vsl::impl::CommandManagerImpl>{}));
        vsl::ComputePhase phase(manager);
        const auto buffer = manager.get_buffer();
        ASSERT_TRUE(CommandAccess::Data(buffer)->recording_context->work);
        const auto retained = CommandAccess::Data(buffer)->recording_context->work->resources;
        vsl::command::ImageReadback command(&destination, image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL);
        EXPECT_THROW(command.invoke(manager.get_pool(), buffer, empty), std::invalid_argument);
        EXPECT_EQ(CommandAccess::Data(buffer)->recording_context->work->resources, retained);
        phase.cancel();
    }
    TEST_F(ImageReadbackCommandVulkanIntegrationTest, CommandKeepsDestinationWhenOriginalHandleIsDestroyedBeforeRecording) {
        auto manager = make_manager();
        auto image = initialized_image(manager);
        std::optional<vsl::command::ImageReadback> command;
        std::optional<vsl::BufferAccessor::LocalBufferHolder> mapped;
        std::weak_ptr<vsl::impl::BufferImpl> storage;
        {
            vsl::Buffer<vsl::MemoryType::TRANSFER_DESTINATION, vsl::MemoryProperty::HOST_VISIBLE> destination(device, std::size_t{16});
            storage = BufferAccess::Owned(destination).share();
            mapped.emplace(destination.data());
            command.emplace(&destination, image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL);
        }
        ASSERT_FALSE(storage.expired());
        {
            vsl::ComputePhase phase(manager);
            phase << *command;
            command.reset();
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        std::array<std::uint32_t, 4> pixels{};
        auto bytes = mapped->read(sizeof(pixels));
        std::memcpy(pixels.data(), bytes.data(), bytes.size());
        EXPECT_EQ(pixels, (std::array<std::uint32_t, 4>{11, 22, 33, 44}));
    }

    TEST_F(ImageReadbackCommandVulkanIntegrationTest, ViewReadbackKeepsItsRangeLogicalConstraintAndSelectedRetention) {
        if (!host_coherent_available())
            GTEST_SKIP() << "HostVisible|HostCoherent memory unavailable";
        const std::array<std::uint32_t, 6> initial{91, 92, 93, 94, 95, 96};
        for (const bool arena : {false, true}) {
            for (const auto retention : {vsl::ResourceRetention::COMPLETION, vsl::ResourceRetention::SUBMISSION}) {
                for (const bool cancel : {false, true}) {
                    std::weak_ptr<void> storage;
                    std::weak_ptr<int> constraint_owner;
                    {
                        auto manager = make_manager().independent(1);
                        auto image = initialized_image(manager);
                        auto phase = manager.start_phase<vsl::ComputePhase>();
                        const auto constraint = std::make_shared<vsl::ResourceUseState>();
                        vsl::BufferRef reference;
                        {
                            const vsl::BufferCreateInfo info{sizeof(initial), vsl::MemoryType::TRANSFER_DESTINATION,
                                                             vsl::MemoryProperty::HOST_VISIBLE | vsl::MemoryProperty::HOST_COHERENT};
                            const auto whole = [&] {
                                if (arena) {
                                    const std::array infos{info};
                                    vsl::lifetime::BufferArena buffers(device, infos);
                                    return buffers.view(0);
                                }
                                return vsl::BufferView::Create(device, info);
                            }();
                            whole.write(std::as_bytes(std::span(initial)));
                            storage = whole.owner();
                            reference = whole.ref();
                            const auto token = std::make_shared<int>(7);
                            constraint_owner = token;
                            const auto selected = whole.view(8, 8);
                            EXPECT_THROW((vsl::command::ImageReadback(selected, image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL).copy.description()), std::out_of_range);
                            EXPECT_THROW((vsl::command::ImageReadback(whole.view(1, 16), image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL).copy.description()), std::invalid_argument);
                            auto command = std::make_unique<vsl::command::ImageReadback>(selected, image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL,
                                                                                         vsl::ImageRegion{1, 0, 1, 2}, vsl::ResourceUse{constraint, token}, retention);
                            command->invoke(manager.get_pool(), manager.get_buffer(), manager);
                            EXPECT_TRUE(constraint->busy());
                            command.reset();
                        }
                        ASSERT_FALSE(storage.expired());
                        EXPECT_FALSE(constraint_owner.expired());
                        if (!cancel) {
                            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                            device.data_->device.waitIdle();
                        }
                        auto observer = storage.lock();
                        ASSERT_TRUE(observer);
                        if (cancel)
                            phase.cancel();
                        std::array<std::uint32_t, 6> actual{};
                        reference.read_into(std::as_writable_bytes(std::span(actual)));
                        const auto expected = cancel ? initial : std::array<std::uint32_t, 6>{91, 92, 22, 44, 95, 96};
                        EXPECT_EQ(actual, expected);
                        EXPECT_FALSE(constraint->busy());
                        observer.reset();
                        EXPECT_EQ(storage.expired(), retention == vsl::ResourceRetention::COMPLETION);
                        EXPECT_EQ(constraint_owner.expired(), retention == vsl::ResourceRetention::COMPLETION);
                    }
                    EXPECT_TRUE(storage.expired());
                    EXPECT_TRUE(constraint_owner.expired());
                }
            }
        }
    }

    TEST_F(ImageReadbackCommandVulkanIntegrationTest, DestinationPollingCanDestroyTheCommandAndContinueCancelOrStartAPeer) {
        const std::array<std::uint32_t, 4> initial{91, 92, 93, 94};
        for (const int action : {0, 1, 2}) {
            auto manager = make_manager().independent(1), other = manager.independent(1);
            auto image = initialized_image(manager);
            vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> destination(device, initial);
            std::optional<vsl::ComputePhase> recording, peer;
            std::unique_ptr<vsl::command::ImageReadback> command;
            bool released = false;
            auto submitted = manager.start_phase<vsl::ComputePhase>();
            vsl_test::DeferredAction deferred([&] {
                command.reset();
                released = true;
                if (action) {
                    recording->cancel();
                    if (action == 2)
                        peer.emplace(other);
                }
            });
            destination.resource_use().record(manager.get_buffer(), vsl::ResourceRetention::COMPLETION);
            manager.get_buffer().retain_until_complete(deferred.owner());
            ASSERT_EQ(submitted.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            device.data_->device.waitIdle();
            command = std::make_unique<vsl::command::ImageReadback>(vsl::BufferView::FromBuffer(destination), image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL);
            recording.emplace(other);
            if (action)
                EXPECT_THROW(command->invoke(other.get_pool(), other.get_buffer(), other), std::logic_error);
            else
                EXPECT_NO_THROW(command->invoke(other.get_pool(), other.get_buffer(), other));
            EXPECT_TRUE(released);
            EXPECT_FALSE(command);
            if (!action) {
                ASSERT_EQ(recording->submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                ASSERT_TRUE(recording->completion().wait());
            } else if (peer) {
                ASSERT_EQ(peer->submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                ASSERT_TRUE(peer->completion().wait());
            }
            std::array<std::uint32_t, 4> actual{};
            destination.read_into(std::as_writable_bytes(std::span(actual)));
            EXPECT_EQ(actual, action ? initial : (std::array<std::uint32_t, 4>{11, 22, 33, 44}));
            recording->cancel();
        }
    }

} // namespace
