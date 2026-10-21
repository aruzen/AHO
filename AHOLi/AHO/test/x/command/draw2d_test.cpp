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
            list.rectangle({0, 0, 8, 8}, green());
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
        EXPECT_EQ(pixel(image, 4, 4), (std::array<unsigned, 4>{0, 255, 0, 255}));
    }

    TEST_F(Renderer2DVulkanIntegrationTest, CommandsRejectOtherManagersAndCancelledPhases) {
        Renderer renderer(device, *pass, AHO_DRAW_SHADER_DIR);
        auto manager = make_manager();
        auto other = make_manager();
        List list;
        list.rectangle({0, 0, 8, 8}, red());
        auto phase = manager.startPhase<vsl::ComputePhase>();
        auto command = renderer.draw(phase, list, 8, 8);
        ASSERT_TRUE(command);
        {
            auto otherPhase = other.startPhase<vsl::ComputePhase>();
            EXPECT_THROW(command->invoke(other.getPool(), other.getBuffer(), other), std::logic_error);
            otherPhase.cancel();
        }
        phase.cancel();
        EXPECT_THROW(command->invoke(manager.getPool(), manager.getBuffer(), manager), std::logic_error);
        EXPECT_THROW(renderer.draw(phase, list, 8, 8), std::invalid_argument);
    }
} // namespace
#endif
