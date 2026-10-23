#include <AHO/x/command/draw2d.hpp>
static_assert(vsl::command::is_command<aho::x::command::Draw2DCommand>);

#ifdef AHO_GPU_TESTS
#include "../../support/drawing2d_fixture.hpp"
#include <AHO/drawer.hpp>

namespace {
    using Renderer2DVulkanIntegrationTest = aho_test::Drawing2DVulkanIntegrationTest;

    TEST_F(Renderer2DVulkanIntegrationTest, DrawStreamRetainsSnapshotUntilSubmissionCompletes) {
        auto manager = make_manager();
        {
            Renderer renderer(device, *pass, AHO_DRAW_SHADER_DIR, 1);
            List list;
            list.rectangle({0, 0, 8, 8}, Green());
            auto phase = manager.startPhase<vsl::ComputePhase>();
            auto cmd = renderer.draw(phase, list, 8, 8);
            ASSERT_TRUE(cmd);
            auto native = phase << vsl::command::RenderPassBegin(*framebuffer);
            aho::DrawStream<vsl::DefaultPhaseStream> stream{native};
            stream << *cmd;
            EXPECT_EQ(stream.phase_stream.pipeline->_data, cmd->getPipeline()._data);
            stream << vsl::command::RenderPassEnd{};
        }
        const auto image = pixels(manager);
        EXPECT_EQ(Pixel(image, 4, 4), (std::array<unsigned, 4>{0, 255, 0, 255}));
    }

    TEST_F(Renderer2DVulkanIntegrationTest, CommandsRejectOtherManagersAndCancelledPhases) {
        Renderer renderer(device, *pass, AHO_DRAW_SHADER_DIR);
        auto manager = make_manager();
        auto other = make_manager();
        List list;
        list.rectangle({0, 0, 8, 8}, Red());
        auto phase = manager.startPhase<vsl::ComputePhase>();
        auto command = renderer.draw(phase, list, 8, 8);
        ASSERT_TRUE(command);
        {
            auto other_phase = other.startPhase<vsl::ComputePhase>();
            EXPECT_THROW(command->invoke(other.getPool(), other.getBuffer(), other), std::logic_error);
            other_phase.cancel();
        }
        phase.cancel();
        EXPECT_THROW(command->invoke(manager.getPool(), manager.getBuffer(), manager), std::logic_error);
        EXPECT_THROW(renderer.draw(phase, list, 8, 8), std::invalid_argument);
    }
    TEST_F(Renderer2DVulkanIntegrationTest, TextureReplacementAndRemovalPreservePreparedSnapshotsAndOwners) {
        auto manager = make_manager();
        {
            Renderer renderer(device, *pass, AHO_DRAW_SHADER_DIR);
            auto phase = manager.startPhase<vsl::ComputePhase>();
            List left;
            left.rectangle({0, 0, 4, 8}, aho::RGBA(1, 1, 1, 1), {2});
            List right;
            right.rectangle({4, 0, 4, 8}, aho::RGBA(1, 1, 1, 1), {2});
            std::optional<aho::x::command::Draw2DCommand> old_command, new_command;
            std::weak_ptr<vsl::_impl::Image_impl> old_storage;
            {
                auto old = texture({255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255});
                auto replacement = texture({0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255});
                vsl::Sampler sampler(device, vsl::SamplingMode::Nearest, vsl::EdgeMode::ClampToEdge);
                old_storage = old._data;
                renderer.set_texture({2}, old, sampler);
                old_command = renderer.draw(phase, left, 8, 8);
                ASSERT_TRUE(old_command);
                renderer.set_texture({2}, replacement, sampler);
                new_command = renderer.draw(phase, right, 8, 8);
                ASSERT_TRUE(new_command);
                EXPECT_TRUE(renderer.erase_texture({2}));
                EXPECT_FALSE(renderer.erase_texture({2}));
                EXPECT_FALSE(renderer.erase_texture({0}));
            }
            EXPECT_FALSE(old_storage.expired());
            auto stream = phase << vsl::command::RenderPassBegin(*framebuffer);
            stream << *old_command << *new_command;
            EXPECT_EQ(stream.pipeline->_data, new_command->getPipeline()._data);
            stream << vsl::command::RenderPassEnd{};
        }
        const auto result = pixels(manager);
        EXPECT_EQ(Pixel(result, 1, 4), (std::array<unsigned, 4>{255, 0, 0, 255}));
        EXPECT_EQ(Pixel(result, 6, 4), (std::array<unsigned, 4>{0, 0, 255, 255}));
    }

    TEST_F(Renderer2DVulkanIntegrationTest, MixedSolidTextureAndSolidBatchesPreserveBlendingAndPipelineState) {
        auto image = texture({255, 0, 0, 128, 255, 0, 0, 128, 255, 0, 0, 128, 255, 0, 0, 128});
        vsl::Sampler sampler(device, vsl::SamplingMode::Nearest, vsl::EdgeMode::ClampToEdge);
        Renderer renderer(device, *pass, AHO_DRAW_SHADER_DIR);
        renderer.set_texture({3}, image, sampler);
        List list;
        list.rectangle({0, 0, 8, 8}, aho::RGBA(0, 0, 1, 1));
        list.rectangle({1, 1, 6, 6}, aho::RGBA(1, 1, 1, 1), {3});
        list.rectangle({2, 2, 4, 4}, Green());
        ASSERT_EQ(list.batches().size(), 3u);
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            auto cmd = renderer.draw(phase, list, 8, 8);
            ASSERT_TRUE(cmd);
            auto stream = phase << vsl::command::RenderPassBegin(*framebuffer);
            stream << *cmd;
            EXPECT_EQ(stream.pipeline->_data, cmd->getPipeline()._data);
            stream << vsl::command::RenderPassEnd{};
        }
        const auto result = pixels(manager);
        EXPECT_EQ(Pixel(result, 0, 0), (std::array<unsigned, 4>{0, 0, 255, 255}));
        const auto blended = Pixel(result, 1, 1);
        EXPECT_NEAR(blended[0], 128, 1);
        EXPECT_NEAR(blended[2], 127, 1);
        EXPECT_EQ(blended[3], 255u);
        EXPECT_EQ(Pixel(result, 4, 4), (std::array<unsigned, 4>{0, 255, 0, 255}));
    }
} // namespace
#endif
