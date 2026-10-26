#ifdef AHO_GPU_TESTS
#include <AHO/command/buffer_upload.hpp>
#include <AHO/x/command/texture_update.hpp>
#include <VSL/vulkan/phase.hpp>
#include <AHO/image_readback.hpp>
#include "support/vulkan_fixture.hpp"
#include <array>
#include <optional>

namespace {
    namespace resource = aho::x::resource;
    class TextureUpdateVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override;

    protected:
        aho::ImagePixels read(vsl::CommandManager manager, const resource::Texture& texture);
        void submit(vsl::ComputePhase& phase);
    };

    bool TextureUpdateVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }

    aho::ImagePixels TextureUpdateVulkanIntegrationTest::read(vsl::CommandManager manager, const resource::Texture& texture) {
        aho::ImageReadback reader(manager);
        if (!reader.capture(texture.image(), vsl::ImageLayout::SHADER_READ_ONLY_OPTIMAL))
            throw std::runtime_error("Readback unavailable");
        return reader.read();
    }

    void TextureUpdateVulkanIntegrationTest::submit(vsl::ComputePhase& phase) {
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
    }

    TEST_F(TextureUpdateVulkanIntegrationTest, InitialPatchClearsUntouchedPixelsAndReportsMetadata) {
        auto manager = make_manager();
        resource::Texture texture(device, 4, 3);
        aho::resource::Buffer<aho::resource::BufferType::GPU_FRIENDLY> upload(device, 64);
        const std::array<std::uint8_t, 8> patch{255, 0, 0, 255, 0, 255, 0, 255};

        upload.assign(std::as_bytes(std::span(patch)));
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << texture.update(upload, {.x = 1, .y = 1, .width = 2, .height = 1});
            EXPECT_TRUE(upload.busy());
            EXPECT_FALSE(upload.wait(0));
            EXPECT_THROW(upload.assign({}), std::logic_error);
            EXPECT_THROW(upload.write({}), std::logic_error);
            EXPECT_THROW(upload.reserve(128), std::logic_error);
            submit(phase);
        }
        EXPECT_TRUE(upload.wait());
        EXPECT_FALSE(upload.busy());
        auto pixels = read(manager, texture);
        ASSERT_EQ(pixels.bytes.size(), 48u);
        for (std::size_t i = 0; i < pixels.bytes.size(); ++i) {
            const auto expected = i >= 20 && i < 28 ? std::byte{patch[i - 20]} : std::byte{};
            EXPECT_EQ(pixels.bytes[i], expected) << i;
        }
    }

    TEST_F(TextureUpdateVulkanIntegrationTest, RepeatedUpdatesPreserveOtherPixelsAndReuseCapacity) {
        auto manager = make_manager();
        resource::Texture texture(device, 2, 2);
        aho::resource::Buffer<aho::resource::BufferType::GPU_FRIENDLY> upload(device, 64);
        const std::array<std::uint8_t, 16> initial{0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255};

        upload.assign(std::as_bytes(std::span(initial)));
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << texture.update(upload);
            submit(phase);
        }

        upload.reserve(128);
        // reserveで元のpayloadを引き継ぎ、同じ内容を別のTextureへ転送できる。
        resource::Texture copied(device, 2, 2);
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << copied.update(upload);
            submit(phase);
        }
        EXPECT_EQ(read(manager, copied).bytes, std::vector<std::byte>(std::as_bytes(std::span(initial)).begin(), std::as_bytes(std::span(initial)).end()));
        const std::array<std::uint8_t, 4> red{255, 0, 0, 255};
        const std::array<std::byte, 4> blank{};

        upload.assign(blank);
        upload.write(std::as_bytes(std::span(red)));
        auto alias = texture;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << alias.update(upload, {.x = 1, .y = 0, .width = 1, .height = 1});
            submit(phase);
        }
        EXPECT_EQ(upload.capacity(), 128u);
        const auto pixels = read(manager, texture);
        for (std::size_t i = 0; i < 16; ++i)
            EXPECT_EQ(pixels.bytes[i], std::byte{i >= 4 && i < 8 ? red[i - 4] : initial[i]});
    }

    TEST_F(TextureUpdateVulkanIntegrationTest, SamePhaseCanCopySeveralRegionsFromOneImmutableUpload) {
        auto manager = make_manager();
        resource::Texture texture(device, 2, 2);
        aho::resource::Buffer<aho::resource::BufferType::GPU_FRIENDLY> upload(device);
        const std::array<std::uint8_t, 8> payload{255, 0, 0, 255, 0, 255, 0, 255};

        upload.assign(std::as_bytes(std::span(payload)));
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            EXPECT_EQ(manager.get_buffer().completion(), phase.completion());
            phase << texture.update(upload, {.width = 1, .height = 1});
            phase << texture.update(upload, {.x = 1, .y = 1, .width = 1, .height = 1}, 4);
            submit(phase);
        }
        EXPECT_FALSE(manager.get_buffer().completion());
        const auto pixels = read(manager, texture);
        for (std::size_t i = 0; i < 16; ++i)
            EXPECT_EQ(pixels.bytes[i], i < 4 ? std::byte{payload[i]} : i >= 12 ? std::byte{payload[i - 8]}
                                                                               : std::byte{});
    }

    TEST_F(TextureUpdateVulkanIntegrationTest, CancelledFirstUpdateCanBeRetriedWithoutAssumingAnImageLayout) {
        auto manager = make_manager();
        resource::Texture texture(device, 2, 2);
        aho::resource::Buffer<aho::resource::BufferType::GPU_FRIENDLY> upload(device);
        const std::array<std::byte, 4> red{std::byte{255}, {}, {}, std::byte{255}};

        upload.assign(red);
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << texture.update(upload, {.width = 1, .height = 1});
            phase.cancel();
        }
        EXPECT_FALSE(upload.busy());
        upload.assign(red);
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << texture.update(upload, {.x = 1, .y = 1, .width = 1, .height = 1});
            submit(phase);
        }

        const auto pixels = read(manager, texture);
        for (std::size_t i = 0; i < 16; ++i)
            EXPECT_EQ(pixels.bytes[i], i < 12 ? std::byte{} : red[i - 12]);
    }

    TEST_F(TextureUpdateVulkanIntegrationTest, InvalidRequestsDoNotClaimTheBufferOrChangeTheTextureState) {
        auto manager = make_manager();
        resource::Texture texture(device, 2, 2);
        aho::resource::Buffer<aho::resource::BufferType::GPU_FRIENDLY> upload(device);
        std::array<std::byte, 4> pixel{};

        upload.assign(pixel);
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            EXPECT_THROW(phase << texture.update(upload), std::out_of_range);
            EXPECT_THROW(phase << texture.update(upload, {.x = 2, .width = 1, .height = 1}), std::out_of_range);
            EXPECT_THROW(phase << texture.update(upload, {.width = 1, .height = 1}, SIZE_MAX), std::invalid_argument);
            EXPECT_FALSE(upload.busy());
            phase << texture.update(upload, {.width = 1, .height = 1});
            auto other = manager.independent();
            auto second = other.start_phase<vsl::ComputePhase>();
            EXPECT_THROW(second << texture.update(upload, {.width = 1, .height = 1}), std::logic_error);
            second.cancel();
            submit(phase);
        }

        auto command = texture.update(upload, {.width = 1, .height = 1});
        EXPECT_THROW(command.invoke(manager.get_pool(), manager.get_buffer(), manager), std::logic_error);
    }

    TEST_F(TextureUpdateVulkanIntegrationTest, CPUToOnGPUUploadFeedsTextureUpdateInTheSamePhase) {
        const std::array<std::byte, 4> pixel{std::byte{11}, std::byte{37}, std::byte{73}, std::byte{255}};
        aho::resource::Buffer<aho::resource::BufferType::GENERAL> cpu;
        cpu.assign(pixel);
        aho::resource::Buffer<aho::resource::BufferType::ON_GPU> gpu(device, 4);
        resource::Texture texture(device, 1, 1);
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << gpu.upload(cpu) << texture.update(gpu);
            submit(phase);
        }
        EXPECT_EQ(read(manager, texture).bytes, (std::vector<std::byte>(pixel.begin(), pixel.end())));
    }

    TEST_F(TextureUpdateVulkanIntegrationTest, CustomVSLBufferUsesTheSameTextureUpdateAndBusyTracking) {
        using Native = vsl::Buffer<vsl::MemoryType::TRANSFER_SOURCE, vsl::MemoryProperty::HOST_VISIBLE>;
        auto source = aho::resource::make_custom_buffer(Native(device, 4));
        const std::array<std::byte, 4> pixel{std::byte{17}, std::byte{37}, std::byte{73}, std::byte{255}};

        source.write(pixel);
        resource::Texture texture(device, 1, 1);
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << texture.update(source);
            EXPECT_TRUE(source.busy());
            EXPECT_THROW(source.write(pixel), std::logic_error);
            submit(phase);
        }
        EXPECT_EQ(read(manager, texture).bytes, (std::vector<std::byte>(pixel.begin(), pixel.end())));
    }
} // namespace
#endif
