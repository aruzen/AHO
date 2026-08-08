#include <aho/resource/buffer.hpp>

#include <gtest/gtest.h>

#include <cstring>
#include <type_traits>

TEST(AHOResourceBuffer, GeneralBufferOwnsRequestedSize)
{
    aho::resource::Buffer<aho::resource::BufferType::General> buffer{16};

    EXPECT_EQ(buffer.size(), 16u);
    EXPECT_NE(buffer.data(), nullptr);
}

TEST(AHOResourceBuffer, GeneralBufferCanStoreBytes)
{
    aho::resource::Buffer<aho::resource::BufferType::General> buffer{4};
    auto *data = static_cast<unsigned char *>(buffer.data());

    data[0] = 0x12;
    data[1] = 0x34;
    data[2] = 0x56;
    data[3] = 0x78;

    EXPECT_EQ(data[0], 0x12);
    EXPECT_EQ(data[1], 0x34);
    EXPECT_EQ(data[2], 0x56);
    EXPECT_EQ(data[3], 0x78);
}

TEST(AHOResourceBuffer, GeneralBufferIsMoveOnly)
{
    using Buffer = aho::resource::Buffer<aho::resource::BufferType::General>;

    static_assert(!std::is_copy_constructible_v<Buffer>);
    static_assert(!std::is_copy_assignable_v<Buffer>);
    static_assert(std::is_move_constructible_v<Buffer>);
    static_assert(std::is_move_assignable_v<Buffer>);
}
