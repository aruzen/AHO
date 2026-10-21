#include <AHO/resource/buffer.hpp>
#include <gtest/gtest.h>
#include <array>
#include <cstring>
#include <optional>
#include <utility>

#ifdef AHO_GPU_TESTS
#include "../support/buffer_customization.hpp"
#include "support/vulkan_fixture.hpp"
#else
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define AHO_BUFFER_TEST_ASAN 1
#endif
#elif defined(__SANITIZE_ADDRESS__)
#define AHO_BUFFER_TEST_ASAN 1
#endif
#ifdef AHO_BUFFER_TEST_ASAN
#include <sanitizer/asan_interface.h>
#endif
#endif

namespace {
    template<typename T>
    concept CompleteBuffer = requires { sizeof(T); };

    template<typename T>
    concept HasCPUWrite = requires(T& b, std::span<const std::byte> bytes) { b.write(bytes); };

    template<typename T>
    concept HasCPUMap = requires(T& b) { b.map(); };

    template<typename T>
    concept HasReserve = requires(T& b) { b.reserve(std::size_t{}); };

    using General = aho::resource::Buffer<aho::resource::BufferType::General>;
    using Host = aho::resource::Buffer<aho::resource::BufferType::GPUFriendly>;
    using Device = aho::resource::Buffer<aho::resource::BufferType::OnGPU>;
    using CustomHost = decltype(aho::resource::make_custom_buffer(std::declval<vsl::StagingBuffer<>>()));
    using CustomDevice = decltype(aho::resource::make_custom_buffer(std::declval<vsl::DeviceLocalBuffer<>>()));
    static_assert(!CompleteBuffer<aho::resource::Buffer<aho::resource::BufferType::Custom>>);
    static_assert(aho::resource::GPUBuffer<Host> && aho::resource::GPUBuffer<Device> && aho::resource::GPUBuffer<CustomHost>);
    static_assert(!aho::resource::GPUBuffer<General>);
    static_assert(HasCPUWrite<Host> && HasCPUMap<Host> && HasReserve<Host>);
    static_assert(!HasCPUWrite<Device> && !HasCPUMap<Device> && !HasReserve<Device>);
    static_assert(HasCPUWrite<CustomHost> && !HasReserve<CustomHost>);
    static_assert(!HasCPUWrite<CustomDevice> && !HasCPUMap<CustomDevice>);
} // namespace

#ifndef AHO_GPU_TESTS
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
            EXPECT_EQ(data[0], 0x5a);
            EXPECT_EQ(data[size - 1], 0x5a);
            EXPECT_EQ(buffer.data(), data);
        }
    }

    INSTANTIATE_TEST_SUITE_P(AllocationBoundaries, GeneralBufferTest, testing::Values(std::size_t{0}, std::size_t{1}, std::size_t{7}, std::size_t{257}));
    TEST(GeneralBufferOwnershipTest, OwningAllocationRequiresNontrivialDestruction) {
        EXPECT_FALSE(std::is_trivially_destructible_v<GeneralBuffer>);
    }
} // namespace

namespace {
    TEST(GeneralBufferOwnershipTest, CopiesKeepTheExistingSharedStorageSemantics) {
        GeneralBuffer source(8);
        auto* storage = static_cast<unsigned char*>(source.data());
        storage[0] = 17;
        GeneralBuffer copy(source);
        GeneralBuffer assigned(3);
        assigned = source;
        EXPECT_EQ(copy.data(), source.data());
        EXPECT_EQ(assigned.data(), source.data());
        EXPECT_EQ(copy.size(), 8u);
        EXPECT_EQ(assigned.size(), 8u);
        static_cast<unsigned char*>(copy.data())[0] = 29;
        EXPECT_EQ(storage[0], 29);
        EXPECT_EQ(static_cast<unsigned char*>(assigned.data())[0], 29);
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
            EXPECT_EQ(static_cast<unsigned char*>(copy->data())[i], 0x5a);
    }

    TEST(GeneralBufferOwnershipTest, MovesKeepTheStorageInTheDestination) {
        GeneralBuffer source(8);
        auto* storage = source.data();
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
        EXPECT_EQ(static_cast<unsigned char*>(assigned.data())[7], 0x6b);
    }
} // namespace
namespace {
    namespace resource = aho::resource;
    TEST(GeneralBufferCapacityTest, ReserveResizeAndAliasesShareStorageWithoutGrowingWithinCapacity) {
        resource::Buffer<resource::BufferType::General> buffer(4);
        EXPECT_EQ(buffer.size(), 4u);
        buffer.reserve(32);
        auto* storage = buffer.bytes().data();
        auto alias = buffer;
        buffer.resize(8);
        EXPECT_EQ(buffer.bytes().data(), storage);
        EXPECT_EQ(alias.size(), 8u);
        EXPECT_GE(alias.capacity(), 32u);
        const std::array<std::byte, 2> bytes{std::byte{17}, std::byte{29}};

        alias.write(bytes, 2);
        EXPECT_EQ(buffer.bytes()[2], bytes[0]);
        const auto& view = buffer;
        EXPECT_EQ(view.bytes()[3], bytes[1]);
        EXPECT_EQ(view.bytes()[7], std::byte{});
        auto moved = std::move(buffer);
        EXPECT_EQ(buffer.size(), 0u);
        EXPECT_TRUE(buffer.bytes().empty());
        EXPECT_EQ(moved.bytes().data(), storage);
    }

    TEST(GeneralBufferCapacityTest, BoundsAndOverlappingWritesPreserveThePayload) {
        resource::Buffer<resource::BufferType::General> buffer(4);
        const std::array<std::byte, 4> initial{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};

        buffer.write(initial);
        buffer.write(buffer.bytes().first(3), 1);
        EXPECT_EQ(buffer.bytes()[0], std::byte{1});
        EXPECT_EQ(buffer.bytes()[1], std::byte{1});
        EXPECT_EQ(buffer.bytes()[2], std::byte{2});
        EXPECT_EQ(buffer.bytes()[3], std::byte{3});
        EXPECT_THROW(buffer.write(initial, 1), std::out_of_range);
        EXPECT_THROW(buffer.write({}, SIZE_MAX), std::out_of_range);
        EXPECT_NO_THROW(buffer.write({}, buffer.size()));
        buffer.resize(0);
        EXPECT_TRUE(buffer.bytes().empty());
        EXPECT_GE(buffer.capacity(), 4u);
    }
} // namespace
namespace {
    TEST(GeneralBufferCapacityTest, AssignmentFromOwnSubrangeAndConstAccessKeepSharedMetadata) {
        General buffer(8);
        const std::array<std::byte, 4> payload{std::byte{11}, std::byte{22}, std::byte{33}, std::byte{44}};

        buffer.assign(payload);
        auto alias = buffer;
        buffer.assign(buffer.bytes().subspan(1, 2));
        EXPECT_EQ(alias.size(), 2u);
        EXPECT_EQ(alias.bytes()[0], payload[1]);
        EXPECT_EQ(alias.bytes()[1], payload[2]);
        const auto& view = alias;
        EXPECT_EQ(view.data(), buffer.data());
        EXPECT_EQ(view.bytes().size(), 2u);
        auto moved = std::move(buffer);
        EXPECT_THROW(buffer.assign(payload), std::logic_error);
        EXPECT_THROW(buffer.reserve(8), std::logic_error);
        EXPECT_THROW(buffer.resize(8), std::logic_error);
        EXPECT_THROW(buffer.write(payload), std::logic_error);
    }
} // namespace
#ifdef AHO_BUFFER_TEST_ASAN
TEST(GeneralBufferOwnershipTest, LastOwnerDestructionReleasesTheArray) {
    void* storage = nullptr;
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

#else
namespace {
    class GPUFriendlyBufferVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    bool GPUFriendlyBufferVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }

    TEST_F(GPUFriendlyBufferVulkanIntegrationTest, LogicalLengthAndReservedCapacityAreIndependent) {
        aho::resource::Buffer<aho::resource::BufferType::GPUFriendly> buffer(device);
        EXPECT_EQ(buffer.size(), 0u);
        EXPECT_EQ(buffer.capacity(), 0u);
        EXPECT_FALSE(buffer.busy());
        EXPECT_TRUE(buffer.wait(0));
        buffer.reserve(64);
        const std::array<std::byte, 8> payload{};

        buffer.assign(payload);
        EXPECT_EQ(buffer.size(), 8u);
        EXPECT_EQ(buffer.capacity(), 64u);
        auto alias = buffer;
        alias.assign(std::span(payload).first(4));
        EXPECT_EQ(buffer.size(), 4u);
        EXPECT_EQ(buffer.capacity(), 64u);
        buffer.reserve(128);
        EXPECT_EQ(alias.capacity(), 128u);
        EXPECT_EQ(alias.size(), 4u);
        buffer.write({});
    }

    TEST_F(GPUFriendlyBufferVulkanIntegrationTest, InvalidWritesDoNotChangeSizeAndEmptyAssignmentKeepsCapacity) {
        aho::resource::Buffer<aho::resource::BufferType::GPUFriendly> buffer(device, 16);
        std::array<std::byte, 4> payload{};

        buffer.assign(payload);
        EXPECT_THROW(buffer.write(payload, 1), std::out_of_range);
        EXPECT_THROW(buffer.write({}, SIZE_MAX), std::out_of_range);
        EXPECT_NO_THROW(buffer.write({}, 4));
        EXPECT_EQ(buffer.size(), 4u);
        buffer.assign({});
        EXPECT_EQ(buffer.size(), 0u);
        EXPECT_EQ(buffer.capacity(), 16u);
        EXPECT_THROW((aho::resource::Buffer<aho::resource::BufferType::GPUFriendly>(vsl::LogicalDeviceAccessor{})), std::invalid_argument);
    }
} // namespace
namespace {
    using aho_test::CustomCounters;
    using aho_test::NativeHost;
    using aho_test::CountingCustomization;

    struct MoveOnlyCustomization {
        std::unique_ptr<int> value = std::make_unique<int>(7);
        MoveOnlyCustomization() = default;
        MoveOnlyCustomization(MoveOnlyCustomization&&) = default;
        MoveOnlyCustomization(const MoveOnlyCustomization&) = delete;
    };

    TEST_F(GPUFriendlyBufferVulkanIntegrationTest, ConstructorsExposeStandardUsageAndReuseTheirMapping) {
        Host source(device, 8);
        EXPECT_EQ(source.size(), 8u);
        EXPECT_EQ(source.capacity(), 8u);
        EXPECT_EQ(source.native().getDevice(), device);
        EXPECT_TRUE(vsl::contain(source.native().getUsage(), vsl::MemoryType::VertexBuffer | vsl::MemoryType::UniformBuffer));
        EXPECT_TRUE(vsl::contain(source.native().getMemoryProperties(), vsl::MemoryProperty::HostVisible));
        std::array<std::byte, 8> expected{};
        expected[3] = std::byte{71};

        source.write(expected);
        const auto* address = source.read(8).data();
        auto alias = source;
        alias.resize(4);
        EXPECT_EQ(source.size(), 4u);
        EXPECT_EQ(alias.read(4).data(), address);
        source.reserve(32);
        EXPECT_EQ(alias.capacity(), 32u);
        EXPECT_EQ(alias.read(4)[3], std::byte{71});
        EXPECT_EQ(source.map().data, source.read(4).data());
        const auto& view = source;
        EXPECT_EQ(view.native().requestedSize(), 32u);
        auto moved = std::move(source);
        EXPECT_EQ(source.size(), 0u);
        EXPECT_THROW(source.binding(), std::logic_error);
        EXPECT_THROW(source.read(0), std::logic_error);
        EXPECT_THROW(source.wait(0), std::logic_error);
        EXPECT_THROW(source.native(), std::logic_error);
        EXPECT_THROW(moved.read(1, 4), std::out_of_range);
        EXPECT_TRUE(moved.read(0, 4).empty());
    }

    TEST_F(GPUFriendlyBufferVulkanIntegrationTest, CustomMappingAndReallocationHooksPreserveDataAndSharedState) {
        auto counters = std::make_shared<CustomCounters>();
        auto custom = aho::resource::make_custom_buffer(NativeHost(device, 4), CountingCustomization{device, counters});
        const std::array<std::byte, 4> expected{std::byte{5}, std::byte{13}, std::byte{29}, std::byte{71}};

        custom.write(expected);
        auto alias = custom;
        custom.reserve(16);
        EXPECT_EQ(counters->maps, 2u);
        EXPECT_EQ(counters->allocations, 1u);
        EXPECT_EQ(alias.capacity(), 16u);
        EXPECT_EQ(alias.size(), 4u);
        EXPECT_EQ(std::memcmp(alias.read(4).data(), expected.data(), 4), 0);
        alias.resize(8);
        EXPECT_EQ(custom.size(), 8u);
        auto moveOnly = aho::resource::make_custom_buffer(NativeHost(device, 4), MoveOnlyCustomization{});
        moveOnly.assign(expected);
        EXPECT_EQ(moveOnly.read(1)[0], expected[0]);
    }

    TEST_F(GPUFriendlyBufferVulkanIntegrationTest, InvalidCustomReallocationLeavesTheOriginalStorageIntact) {
        struct TooSmallAllocation {
            vsl::LogicalDeviceAccessor device;
            NativeHost reallocate(const NativeHost&, std::size_t) { return NativeHost(device, 1); }
        };

        auto custom = aho::resource::make_custom_buffer(NativeHost(device, 4), TooSmallAllocation{device});
        const std::array<std::byte, 4> payload{std::byte{11}, std::byte{22}, std::byte{33}, std::byte{44}};

        custom.write(payload);
        const auto owner = custom.binding().owner();
        EXPECT_THROW(custom.reserve(8), std::invalid_argument);
        EXPECT_EQ(custom.binding().owner(), owner);
        EXPECT_EQ(custom.capacity(), 4u);
        EXPECT_EQ(std::memcmp(custom.read(4).data(), payload.data(), 4), 0);
    }

    TEST_F(GPUFriendlyBufferVulkanIntegrationTest, CustomizationContextOutlivesTheWrappedStorageDestructor) {
        struct ContextStorage : NativeHost {
            std::weak_ptr<int> context;
            std::shared_ptr<bool> destroyedWithoutContext;
            ContextStorage(vsl::LogicalDeviceAccessor device, const std::shared_ptr<int>& context, std::shared_ptr<bool> result)
                : NativeHost(device, 4), context(context), destroyedWithoutContext(std::move(result)) {}
            ContextStorage(ContextStorage&&) = default;
            ContextStorage(const ContextStorage&) = delete;
            ~ContextStorage() override {
                if (_data && context.expired())
                    *destroyedWithoutContext = true;
            }
        };

        struct ContextCustomization {
            std::shared_ptr<int> context;
        };

        auto result = std::make_shared<bool>(false);
        {
            auto context = std::make_shared<int>(7);
            auto custom = aho::resource::make_custom_buffer(ContextStorage(device, context, result), ContextCustomization{context});
            context.reset();
        }
        EXPECT_FALSE(*result);
    }

} // namespace
#endif
