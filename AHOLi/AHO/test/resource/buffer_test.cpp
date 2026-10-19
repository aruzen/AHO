#include <gtest/gtest.h>
#include <AHO/resource/buffer.hpp>
#include <cstring>
#include <optional>
#include <utility>

namespace {
    using GeneralBuffer = aho::resource::Buffer<aho::resource::BufferType::General>;
    class GeneralBufferTest : public testing::TestWithParam<std::size_t> {};
    TEST_P(GeneralBufferTest, ConstructorReportsRequestedSizeAndWritableStorage) {
        const auto size = GetParam();
        GeneralBuffer buffer(size);
        EXPECT_EQ(buffer.size(), size);
        if (size > 0) {
            auto *data = static_cast<unsigned char *>(buffer.data());
            ASSERT_NE(data, nullptr);
            std::memset(data, 0x5a, size);
            EXPECT_EQ(data[0], 0x5a);
            EXPECT_EQ(data[size - 1], 0x5a);
            EXPECT_EQ(buffer.data(), data);
        }
    }
    INSTANTIATE_TEST_SUITE_P(AllocationBoundaries, GeneralBufferTest, testing::Values(std::size_t{0}, std::size_t{1}, std::size_t{7}, std::size_t{257}));
    TEST(GeneralBufferOwnershipTest, OwningAllocationRequiresNontrivialDestruction) {
        EXPECT_FALSE(std::is_trivially_destructible_v<GeneralBuffer>);
    }
}

namespace {
    TEST(GeneralBufferOwnershipTest, CopiesKeepTheExistingSharedStorageSemantics) {
        GeneralBuffer source(8);
        auto *storage = static_cast<unsigned char *>(source.data());
        storage[0] = 17;
        GeneralBuffer copy(source);
        GeneralBuffer assigned(3);
        assigned = source;
        EXPECT_EQ(copy.data(), source.data());
        EXPECT_EQ(assigned.data(), source.data());
        EXPECT_EQ(copy.size(), 8u);
        EXPECT_EQ(assigned.size(), 8u);
        static_cast<unsigned char *>(copy.data())[0] = 29;
        EXPECT_EQ(storage[0], 29);
        EXPECT_EQ(static_cast<unsigned char *>(assigned.data())[0], 29);
    }
    TEST(GeneralBufferOwnershipTest, SharedStorageOutlivesTheOriginalBuffer) {
        std::optional<GeneralBuffer> copy;
        {
            GeneralBuffer source(4);
            std::memset(source.data(), 0x5a, source.size());
            copy.emplace(source);
        }
        ASSERT_EQ(copy->size(), 4u);
        for (std::size_t i = 0; i < copy->size(); ++i)
            EXPECT_EQ(static_cast<unsigned char *>(copy->data())[i], 0x5a);
    }
    TEST(GeneralBufferOwnershipTest, MovesKeepTheStorageInTheDestination) {
        GeneralBuffer source(8);
        auto *storage = source.data();
        std::memset(storage, 0x6b, 8);
        GeneralBuffer moved(std::move(source));
        EXPECT_EQ(moved.data(), storage);
        EXPECT_EQ(moved.size(), 8u);
        EXPECT_EQ(source.size(), 0u);
        EXPECT_EQ(source.data(), nullptr);
        GeneralBuffer assigned(2);
        assigned = std::move(moved);
        EXPECT_EQ(assigned.data(), storage);
        EXPECT_EQ(moved.size(), 0u);
        EXPECT_EQ(moved.data(), nullptr);
        EXPECT_EQ(assigned.size(), 8u);
        EXPECT_EQ(static_cast<unsigned char *>(assigned.data())[7], 0x6b);
    }
} // namespace
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define AHO_BUFFER_TEST_ASAN 1
#endif
#elif defined(__SANITIZE_ADDRESS__)
#define AHO_BUFFER_TEST_ASAN 1
#endif
#ifdef AHO_BUFFER_TEST_ASAN
#include <sanitizer/asan_interface.h>
TEST(GeneralBufferOwnershipTest, LastOwnerDestructionReleasesTheArray) {
    void *storage = nullptr;
    {
        GeneralBuffer source(8);
        storage = source.data();
        {
            GeneralBuffer copy(source);
            EXPECT_EQ(__asan_address_is_poisoned(storage), 0);
        }
        EXPECT_EQ(__asan_address_is_poisoned(storage), 0);
    }
    EXPECT_NE(__asan_address_is_poisoned(storage), 0);
}
#undef AHO_BUFFER_TEST_ASAN
#endif
