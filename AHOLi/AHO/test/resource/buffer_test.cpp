#include <gtest/gtest.h>
#include <AHO/resource/buffer.hpp>
#include <cstring>

namespace {
using GeneralBuffer = aho::resource::Buffer<aho::resource::BufferType::General>;
class GeneralBufferTest : public testing::TestWithParam<std::size_t> {};
TEST_P(GeneralBufferTest, ConstructorReportsRequestedSizeAndWritableStorage) {
    const auto size = GetParam();
    GeneralBuffer buffer(size);
    EXPECT_EQ(buffer.size(), size);
    if (size > 0) {
        auto* data = static_cast<unsigned char*>(buffer.data());
        ASSERT_NE(data, nullptr);
        std::memset(data, 0x5a, size);
        EXPECT_EQ(data[0], 0x5a); EXPECT_EQ(data[size - 1], 0x5a);
        EXPECT_EQ(buffer.data(), data);
    }
}
INSTANTIATE_TEST_SUITE_P(AllocationBoundaries, GeneralBufferTest, testing::Values(std::size_t{0}, std::size_t{1}, std::size_t{7}, std::size_t{257}));
TEST(GeneralBufferOwnershipTest, OwningAllocationRequiresNontrivialDestruction) {
    EXPECT_FALSE(std::is_trivially_destructible_v<GeneralBuffer>);
}
}
