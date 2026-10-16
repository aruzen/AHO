#include <gtest/gtest.h>
#include "support/compile_probe.hpp"
namespace {
TEST(ImageCopyDefinitionTest, DISABLED_CopyByBufferReturnsItsDeclaredBooleanResult) {
    const auto result = vsl_test::compile_probe(R"cpp(
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vsl.hpp>
#include <VSL/vulkan/post_definition.hpp>
void probe(vsl::Image<vsl::ImageType::TransferDst>& image, vsl::CommandManager manager,
           vsl::StagingBuffer<vsl::MemoryType::TransferSource>& buffer) {
    (void)image.copyByBuffer(manager,buffer);
}
)cpp");
    EXPECT_EQ(result.status, 0) << result.diagnostics;
}
} // namespace
