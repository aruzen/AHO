#include "support/vulkan_fixture.hpp"
#include <VSL/vulkan/commands/copy_buffer.hpp>

#include <array>

namespace {
    class ArenaVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    bool ArenaVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }

    using HostBuffer = vsl::Buffer<vsl::MemoryType::TRANSFER_SOURCE | vsl::MemoryType::TRANSFER_DESTINATION | vsl::MemoryType::UNIFORM_BUFFER,
                                   vsl::MemoryProperty::HOST_VISIBLE>;
    constexpr std::size_t ARENA_BYTES = 4096;

    TEST_F(ArenaVulkanIntegrationTest, SamplerIsPlacedAndDestroyedWithTheArena) {
        std::weak_ptr<vsl::impl::SamplerImpl> weak;
        {
            vsl::Arena arena(ARENA_BYTES);
            vsl::Sampler sampler(arena, device, vsl::SamplingMode::NEAREST, vsl::EdgeMode::CLAMP_TO_EDGE);
            EXPECT_TRUE(sampler.data_->sampler);
            EXPECT_EQ(sampler.data_->device, device.data_);
            EXPECT_EQ(arena.count(), 1u);
            weak = sampler.data_;
        }
        EXPECT_TRUE(weak.expired());
        EXPECT_FALSE(has_validation_errors());
    }

    TEST_F(ArenaVulkanIntegrationTest, BufferInArenaBehavesLikeAHeapBuffer) {
        vsl::Arena arena(ARENA_BYTES);
        HostBuffer placed(arena, device, 16);
        HostBuffer heap(device, 16);
        EXPECT_EQ(placed.requested_size(), heap.requested_size());
        EXPECT_EQ(placed.get_usage(), heap.get_usage());
        EXPECT_EQ(arena.count(), 1u);

        const std::array<std::byte, 4> bytes{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
        placed.data().write(bytes);
        const auto read = placed.data().read(4);
        EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), read.begin()));
    }

    TEST_F(ArenaVulkanIntegrationTest, PendingGpuWorkKeepsTheWholeArenaAlive) {
        std::weak_ptr<vsl::impl::BufferImpl> weak_source;
        {
            auto manager = make_manager();
            HostBuffer destination(device, 8);
            destination.data().write(std::array<std::byte, 8>{});
            auto phase = manager.start_phase<vsl::ComputePhase>();
            {
                vsl::Arena arena(ARENA_BYTES);
                HostBuffer source(arena, device, 8);
                source.data().write(std::array<std::byte, 8>{std::byte{9}, std::byte{9}, std::byte{9}, std::byte{9},
                                                             std::byte{9}, std::byte{9}, std::byte{9}, std::byte{9}});
                weak_source = source.data_;
                vsl::command::CopyBuffer copy(&source, &destination, 8);
                copy.validate();
                phase << copy;
            }
            // 既存のCompletion::retainがarena全体の所有を保持する。
            EXPECT_FALSE(weak_source.expired());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
            EXPECT_EQ(destination.data().read(8)[0], std::byte{9});
        }
        // submissionの破棄で保持が外れ、arenaも破棄される。
        EXPECT_TRUE(weak_source.expired());
        EXPECT_FALSE(has_validation_errors());
    }

    TEST_F(ArenaVulkanIntegrationTest, DescriptorSetLayoutAndBoundBufferInOneArenaHaveNoCycle) {
        using vsl::graphic_resource::Type;
        vsl::GraphicResourceManager manager(device);
        auto pool = manager.make({{Type::UNIFORM_BUFFER, 4}});
        std::weak_ptr<vsl::impl::GraphicResourceImpl> weak_set;
        std::weak_ptr<vsl::impl::ResourceBindingLayoutImpl> weak_layout;
        {
            vsl::Arena arena(ARENA_BYTES);
            vsl::graphic_resource::BindingLayout layout(arena, device, {{0, Type::UNIFORM_BUFFER, vsl::ShaderFlag::COMPUTE}});
            auto [ok, set] = pool.bind(arena, layout);
            ASSERT_TRUE(ok);
            HostBuffer buffer(arena, device, 16);
            set.update(&buffer);
            EXPECT_EQ(arena.count(), 3u);

            // 同じarenaの参照は非所有、別所有のpoolは所有する。
            EXPECT_EQ(set.data_->layout.use_count(), 0);
            EXPECT_GT(set.data_->pool.use_count(), 0);
            // 外へ渡すpoolは所有を持つ。
            EXPECT_EQ(set.get_pool().data_, pool.data_);

            weak_set = set.data_;
            weak_layout = layout.data_;
        }
        EXPECT_TRUE(weak_set.expired());
        EXPECT_TRUE(weak_layout.expired());
        EXPECT_FALSE(has_validation_errors());
    }

    TEST_F(ArenaVulkanIntegrationTest, HeapSetKeepsAnArenaBufferAliveAndArenaSetKeepsAHeapBufferAlive) {
        using vsl::graphic_resource::Type;
        vsl::GraphicResourceManager manager(device);
        auto pool = manager.make({{Type::UNIFORM_BUFFER, 4}});
        vsl::graphic_resource::BindingLayout layout(device, {{0, Type::UNIFORM_BUFFER, vsl::ShaderFlag::COMPUTE}});

        auto [heap_ok, heap_set] = pool.bind(layout);
        ASSERT_TRUE(heap_ok);
        std::weak_ptr<vsl::impl::BufferImpl> weak_arena_buffer;
        {
            vsl::Arena arena(ARENA_BYTES);
            HostBuffer buffer(arena, device, 16);
            heap_set.update(&buffer);
            weak_arena_buffer = buffer.data_;
        }
        EXPECT_FALSE(weak_arena_buffer.expired());

        std::weak_ptr<vsl::impl::BufferImpl> weak_heap_buffer;
        vsl::Arena arena(ARENA_BYTES);
        auto [arena_ok, arena_set] = pool.bind(arena, layout);
        ASSERT_TRUE(arena_ok);
        {
            HostBuffer buffer(device, 16);
            arena_set.update(&buffer);
            weak_heap_buffer = buffer.data_;
        }
        EXPECT_FALSE(weak_heap_buffer.expired());

        heap_set = {};
        EXPECT_TRUE(weak_arena_buffer.expired());
        arena_set = {};
        arena = vsl::Arena(16);
        EXPECT_TRUE(weak_heap_buffer.expired());
        EXPECT_FALSE(has_validation_errors());
    }

    TEST_F(ArenaVulkanIntegrationTest, ImageSamplerAndCombinedDescriptorInOneArenaHaveNoCycle) {
        using vsl::graphic_resource::Type;
        using SampledImage = vsl::Image<vsl::ImageType::SAMPLED | vsl::ImageType::TRANSFER_DST>;
        vsl::GraphicResourceManager manager(device);
        auto pool = manager.make({{Type::COMBINED_IMAGE_SAMPLER, 4}});
        std::weak_ptr<vsl::impl::ImageImpl> weak_image;
        std::weak_ptr<vsl::impl::GraphicResourceImpl> weak_set;
        {
            vsl::Arena arena(ARENA_BYTES);
            SampledImage image(arena, device, 4, 4, vsl::data_format::UNSIGNED_INT8_RGBA);
            vsl::Sampler sampler(arena, device, vsl::SamplingMode::NEAREST, vsl::EdgeMode::CLAMP_TO_EDGE);
            vsl::graphic_resource::BindingLayout layout(arena, device, {{0, Type::COMBINED_IMAGE_SAMPLER, vsl::ShaderFlag::COMPUTE}});
            auto [ok, set] = pool.bind(arena, layout);
            ASSERT_TRUE(ok);
            set.update(image, sampler);
            EXPECT_EQ(arena.count(), 4u);
            EXPECT_TRUE(image.data_->view);
            weak_image = image.data_;
            weak_set = set.data_;
        }
        EXPECT_TRUE(weak_set.expired());
        EXPECT_TRUE(weak_image.expired());
        EXPECT_FALSE(has_validation_errors());
    }
} // namespace
