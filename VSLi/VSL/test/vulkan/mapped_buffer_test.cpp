#include "support/render_fixture.hpp"
#include <VSL/vulkan/mapped_buffer.hpp>
#include <VSL/vulkan/commands/image_readback.hpp>
namespace {
    class MappedBufferVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(MappedBufferVulkanIntegrationTest, ReusesMappingAndReadsBoundedRangesAfterGpuCompletion) {
        auto manager = make_manager();
        VSL_NAMESPACE::MappedBuffer destination(device, width * height * 4);
        const std::byte* address = nullptr;
        for (unsigned frame = 0; frame < 8; ++frame) {
            VSL_NAMESPACE::ComputePhase phase(manager);
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{1, 0, 0, 1}})
                  << VSL_NAMESPACE::command::RenderPassEnd{}
                  << VSL_NAMESPACE::command::ImageReadback(destination, *color, VSL_NAMESPACE::ImageLayout::General);
            ASSERT_EQ(phase.submit(), VSL_NAMESPACE::ComputePhase::SubmitResult::Success);
            ASSERT_TRUE(phase.completion().wait());
            auto pixels = destination.read(4);
            EXPECT_EQ(pixels[0], std::byte{255});
            EXPECT_EQ(pixels[1], std::byte{0});
            EXPECT_EQ(pixels[3], std::byte{255});
            if (address)
                EXPECT_EQ(pixels.data(), address);
            address = pixels.data();
        }
        auto alias = destination;
        EXPECT_EQ(alias.read(4).data(), address);
        EXPECT_EQ(alias.read(3, 1)[2], std::byte{255});
        EXPECT_TRUE(alias.read(0, alias.size()).empty());
        EXPECT_THROW(destination.read(1, destination.size()), std::out_of_range);
        EXPECT_THROW(destination.read(SIZE_MAX, 1), std::out_of_range);
        EXPECT_THROW((VSL_NAMESPACE::MappedBuffer(device, 0)), std::invalid_argument);
        VSL_NAMESPACE::MappedBuffer storage(device, width * height * 4, VSL_NAMESPACE::MemoryType::StorageBuffer);
        EXPECT_THROW((VSL_NAMESPACE::command::ImageReadback(storage, *color, VSL_NAMESPACE::ImageLayout::General)), std::invalid_argument);
    }
    TEST_F(MappedBufferVulkanIntegrationTest, InvalidationPathAlignsPartialRangesAndKeepsMapping) {
        auto manager = make_manager();
        VSL_NAMESPACE::MappedBuffer destination(device, width * height * 4);
        {
            VSL_NAMESPACE::ComputePhase phase(manager);
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{1, 0, 0, 1}})
                  << VSL_NAMESPACE::command::RenderPassEnd{}
                  << VSL_NAMESPACE::command::ImageReadback(destination, *color, VSL_NAMESPACE::ImageLayout::General);
            phase.submit();
            ASSERT_TRUE(phase.completion().wait());
        }
        // コヒーレントメモリしかない環境でも、キャッシュ無効化範囲のアラインメントを確かめる。
        // 実際の非コヒーレントメモリの動作を再現する検証ではない。
        destination._data->coherent = false;
        destination._data->atomSize = physical._data->device.getProperties().limits.nonCoherentAtomSize;
        auto partial = destination.read(3, 1);
        EXPECT_EQ(partial[0], std::byte{0});
        EXPECT_EQ(partial[2], std::byte{255});
        EXPECT_EQ(destination.read(1, destination.size() - 1)[0], std::byte{255});
    }
    TEST_F(MappedBufferVulkanIntegrationTest, RecordedCommandKeepsMappedBufferAliveUntilCompletionOwnerReleasesIt) {
        auto manager = make_manager();
        std::weak_ptr<VSL_NAMESPACE::_impl::MappedBuffer_impl> weak;
        {
            VSL_NAMESPACE::ComputePhase phase(manager);
            {
                VSL_NAMESPACE::MappedBuffer destination(device, width * height * 4);
                weak = destination._data;
                phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer) << VSL_NAMESPACE::command::RenderPassEnd{}
                      << VSL_NAMESPACE::command::ImageReadback(destination, *color, VSL_NAMESPACE::ImageLayout::General);
            }
            EXPECT_FALSE(weak.expired());
            phase.submit();
            EXPECT_TRUE(phase.completion().wait());
        }
        {
            VSL_NAMESPACE::ComputePhase next(manager);
            next.submit();
            EXPECT_TRUE(next.completion().wait());
        }
        EXPECT_TRUE(weak.expired());
    }
    TEST_F(MappedBufferVulkanIntegrationTest, IndependentManagerHasOwnBuffersOnSameDeviceAndQueue) {
        auto producer = make_manager();
        auto transfer = producer.independent();
        EXPECT_EQ(transfer.getDevice(), producer.getDevice());
        EXPECT_NE(transfer.getPool()._data, producer.getPool()._data);
        EXPECT_EQ(transfer.getBuffer().getSize(), 1u);
        EXPECT_EQ(transfer._data->graphicsQueue, producer._data->graphicsQueue);
        auto producerPhase = producer.startPhase<VSL_NAMESPACE::ComputePhase>();
        {
            auto transferPhase = transfer.startPhase<VSL_NAMESPACE::ComputePhase>();
            transferPhase.submit();
            EXPECT_TRUE(transferPhase.completion().wait());
        }
        EXPECT_EQ(producerPhase.state, VSL_NAMESPACE::ComputePhase::State::WaitSubmit);
        producerPhase.cancel();
        EXPECT_THROW(producer.independent(0), std::invalid_argument);
    }
} // namespace
